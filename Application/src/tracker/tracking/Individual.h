#ifndef _FISHP_H
#define _FISHP_H

#include <types.h>
#include <tracker/misc/idx_t.h>
#include <gui/colors.h>
#include <misc/Blob.h>
#include "Posture.h"
#include "MotionRecord.h"
#include <misc/Median.h>
#include <gui/types/Basic.h>
#include <misc/Image.h>
#include <pv.h>

#include <tracking/DetectTag.h>
#include <misc/Timer.h>

#include <tracking/PairingGraph.h>
#include <tracking/IndividualCache.h>
#include <tracking/PPFrame.h>
#include <misc/ranges.h>

#define DEBUG_ORIENTATION false

namespace gui { class Fish; }
namespace cmn { class Data; }
namespace Output { class ResultsFormat; class TrackingResults; }
namespace track { class Individual; }
namespace mem { struct IndividualMemoryStats; }

namespace track {

enum class Reasons {
    None = 0,
    LostForOneFrame = 1,
    TimestampTooDifferent = 2,
    ProbabilityTooSmall = 4,
    ManualMatch = 8,
    WeirdDistance = 16,
    NoBlob = 32,
    MaxSegmentLength = 64
    
};

constexpr std::array<const char*, 8> ReasonsNames {
    "None",
    "LostForOneFrame",
    "TimestampTooDifferent",
    "ProbabilityTooSmall",
    "ManualMatch",
    "WeirdDistance",
    "NoBlob",
    "MaxSegmentLength"
};

    template<typename Iterator, typename T>
        requires _is_smart_pointer<typename Iterator::value_type>
    Iterator find_frame_in_sorted_segments(Iterator start, Iterator end, T object, typename std::enable_if< !is_pair<typename Iterator::value_type>::value, void* >::type = nullptr) {
        if(start != end) {
            auto it = std::upper_bound(start, end, object, [](T o, const auto& ptr) -> bool {
                return o < ptr->start();
            });
            
            if((it == end || it != start) && (*(--it))->start() == object)
            {
                return it;
            }
        }
        
        return end;
    }

    template<typename Iterator, typename T>
        requires (!_is_smart_pointer<typename Iterator::value_type>)
    Iterator find_frame_in_sorted_segments(Iterator start, Iterator end, T object, typename std::enable_if< !is_pair<typename Iterator::value_type>::value, void* >::type = nullptr) {
        if(start != end) {
            auto it = std::upper_bound(start, end, object, [](T o, const auto& ptr) -> bool {
                return o < ptr.frames.start;
            });
            
            if((it == end || it != start) && (*(--it)).frames.start == object)
            {
                return it;
            }
        }
        
        return end;
    }

    template<typename Iterator, typename T>
    Iterator find_frame_in_sorted_segments(Iterator start, Iterator end, T object, typename std::enable_if< is_pair<typename Iterator::value_type>::value, void* >::type = nullptr) {
        if(start != end) {
            auto it = std::upper_bound(start, end, object, [](T o, const auto& ptr) -> bool {
                return o < ptr.first;
            });
            
            if((it == end || it != start) && (--it)->first == object)
            {
                return it;
            }
        }
        
        return end;
    }
    
    class Identity {
    protected:
        GETTER_SETTER(gui::Color, color)
        Idx_t _myID;
        std::string _name;
        GETTER_SETTER(bool, manual)
        
    public:
        static void set_running_id(Idx_t value);
        static Idx_t running_id();
        Identity(Idx_t myID = Idx_t());
        Idx_t ID() const { return _myID; }
        void set_ID(Idx_t val) {
            _color = ColorWheel(val).next();
            _myID = val;
            _name = Meta::toStr(_myID);
        }
        const std::string& raw_name();
        std::string raw_name() const;
        std::string name() const;
        std::string toStr() const {
            return name();
        }
        
        friend class Output::TrackingResults;
    };

#if DEBUG_ORIENTATION
    struct OrientationProperties {
        Frame_t frame;
        float original_angle;
        bool flipped_because_previous;
        
        OrientationProperties(Frame_t frame = -1, float original_angle = 0, bool flipped_because_previous = false)
            : frame(frame),
              original_angle(original_angle),
              flipped_because_previous(flipped_because_previous)
        {
            
        }
    };
#endif
    
    class Individual {
    protected:
        friend class Output::ResultsFormat;
        friend class cmn::Data;
        friend struct mem::IndividualMemoryStats;
        
        //! An identity that is maintained
        Identity _identity;
        
        //! misc warnings
        Timer _warned_normalized_midline;
        
    public:
        //! Stuff that belongs together and is definitely
        //! present in every frame
        struct BasicStuff {
            Frame_t frame;
            
            MotionRecord centroid;
            //MotionRecord* weighted_centroid;
            uint64_t thresholded_size;
            pv::CompressedBlob blob;
            pv::BlobPtr pixels;
            
            BasicStuff()
                : frame(Frame_t::invalid),
                  thresholded_size(0)
            {}
            
        };

    protected:
        //! dense array of all the basic stuff we want to save
        GETTER(std::vector<std::unique_ptr<BasicStuff>>, basic_stuff)
        GETTER(std::vector<default_config::matching_mode_t::Class>, matched_using)
        
    public:
        //! Stuff that is only present if postures are
        //! calculated and present in the given frame.
        //! (There are no frame_segments available for pre-sorting requests)
        struct PostureStuff {
            static constexpr float infinity = cmn::infinity<float>();
            Frame_t frame;
            
            MotionRecord* head;
            MotionRecord* centroid_posture;
            Midline::Ptr cached_pp_midline;
            MinimalOutline::Ptr outline;
            float posture_original_angle;
            float midline_angle, midline_length;
            //!TODO: consider adding processed midline_angle and length
            
            PostureStuff()
                : head(nullptr), centroid_posture(nullptr), posture_original_angle(infinity), midline_angle(infinity), midline_length(infinity)
            {}
            
            ~PostureStuff();
            bool cached() const { return posture_original_angle != infinity; }
        };
        
    protected:
        //! dense array of all posture related stuff we are saving
        GETTER(std::vector<std::unique_ptr<PostureStuff>>, posture_stuff)
        Frame_t _last_posture_added;
        
    public:
        struct SegmentInformation : public FrameRange {
            std::vector<long_t> basic_index;
            std::vector<long_t> posture_index;
            uint32_t error_code = std::numeric_limits<uint32_t>::max();
            
            SegmentInformation(const Range<Frame_t>& range = Range<Frame_t>(Frame_t(), Frame_t()),
                               Frame_t first_usable = Frame_t())
                : FrameRange(range, first_usable)
            {}
            
            void add_basic_at(Frame_t frame, long_t gdx);
            void add_posture_at(std::unique_ptr<PostureStuff>&& stuff, Individual* fish); //long_t gdx);
            //void remove_frame(long_t);
            
            long_t basic_stuff(Frame_t frame) const;
            long_t posture_stuff(Frame_t frame) const;
            
            constexpr bool overlaps(const SegmentInformation& v) const {
                return contains(v.start()) || contains(v.end())
                    || v.contains(start()) || v.contains(end())
                    || v.start() == end() || start() == v.end();
            }
            
            constexpr bool operator<(const SegmentInformation& other) const {
                return range < other.range;
            }
            
            constexpr bool operator<(Frame_t frame) const {
                return range.start < frame;
            }
        };
        
    protected:
        GETTER(std::set<Frame_t>, manually_matched)
        std::set<Frame_t> automatically_matched;
        
#if DEBUG_ORIENTATION
        std::map<long_t, OrientationProperties> _why_orientation;
#endif
        ska::bytell_hash_map<Frame_t, std::map<long_t, std::pair<void*, std::function<void(void*)>>>> _custom_data;
        
        //! A frame index is pushed here, if the previous frame was not the current frame - 1 (e.g. frames are missing)
    public:
        using segment_map = std::vector<std::shared_ptr<SegmentInformation>>;
        segment_map::const_iterator find_segment_with_start(Frame_t frame) const;
        using small_segment_map = std::map<Frame_t, FrameRange>;
        
    protected:
        GETTER(segment_map, frame_segments)
        GETTER(small_segment_map, recognition_segments)
        
        //! Contains a map with individual -> probability for the blob that has been
        //  assigned to this individual.
        std::map<Frame_t, std::tuple<size_t, std::map<Idx_t, float>>> average_recognition_segment;
        std::map<Frame_t, std::tuple<size_t, std::map<Idx_t, float>>> average_processed_segment;
        
        //! Contains a map from fish id to probability that averages over
        //  all available segments when "check identities" was last clicked
        std::map<Idx_t, float> _average_recognition;
        GETTER(size_t, average_recognition_samples)
        
        Frame_t _startFrame, _endFrame;
        
    public:
        //! These data are generated in order to reduce work-load
        //  on a per-frame basis. They need to be regenerated when
        //  frames are removed.
        struct LocalCache {
            robin_hood::unordered_map<Frame_t, Vec2> _current_velocities;
            Vec2 _current_velocity;
            std::vector<Vec2> _v_samples;
            
            float _midline_length;
            uint64_t _midline_samples;
            
            float _outline_size;
            uint64_t _outline_samples;
            
            void regenerate(Individual*);
            
        private:
            void clear();
            
        public:
            Vec2 add(Frame_t frame, const MotionRecord*);
            void add(const PostureStuff&);
            
            LocalCache()
                : _midline_length(0), _midline_samples(0),
                  _outline_size(0), _outline_samples(0)
            {}
            
        };

        struct QRCode {
            Frame_t frame;
            pv::BlobPtr _blob;
        };
        
    protected:
        LocalCache _local_cache;
        std::map<void*, std::function<void(Individual*)>> _delete_callbacks;
        
        //! Segment start to Tag
        std::map<Frame_t, std::multiset<tags::Tag>> _best_images;

#if !COMMONS_NO_PYTHON
        ska::bytell_hash_map<Frame_t, std::vector<QRCode>> _qrcodes;
        mutable std::mutex _qrcode_mutex;
        ska::bytell_hash_map<Frame_t, std::tuple<int64_t, float>> _qrcode_identities;
        Frame_t _last_requested_qrcode;
#endif
        
    public:
#if !COMMONS_NO_PYTHON
        std::tuple<int64_t, float> qrcode_at(Frame_t segment_start) const;
        ska::bytell_hash_map<Frame_t, std::tuple<int64_t, float>> qrcodes() const;
        bool add_qrcode(Frame_t frameIndex, pv::BlobPtr&&);
#endif

        float midline_length() const;
        size_t midline_samples() const;
        float outline_size() const;
        
        void add_tag_image(const tags::Tag& tag);
        const std::multiset<tags::Tag>* has_tag_images_for(Frame_t frameIndex) const;
        std::set<Frame_t> added_postures;
        CacheHints _hints;
        
    public:
        Individual(Identity&& id = Identity());
        ~Individual();
        
#if DEBUG_ORIENTATION
        OrientationProperties why_orientation(Frame_t frame) const;
#endif
        
        void add_custom_data(Frame_t frame, long_t id, void* ptr, std::function<void(void*)> fn_delete) {
            auto it = _custom_data[frame].find(id);
            if(it != _custom_data[frame].end()) {
                FormatWarning("Custom data with id ", id," already present in frame ",frame,".");
                it->second.second(it->second.first);
            }
            _custom_data[frame][id] = { ptr, fn_delete };
        }
        
        void * custom_data(Frame_t frame, long_t id) const {
            auto it = _custom_data.find(frame);
            if(it == _custom_data.end())
                return NULL;
            
            auto it1 = it->second.find(id);
            if(it1 != it->second.end()) {
                return it1->second.first;
            }
            
            return NULL;
        }
        
        const decltype(_identity)& identity() const { return _identity; }
        decltype(_identity)& identity() { return _identity; }
        
        int64_t add(const FrameProperties*, Frame_t frameIndex, const PPFrame& frame, const pv::BlobPtr& blob, Match::prob_t current_prob, default_config::matching_mode_t::Class);
        void remove_frame(Frame_t frameIndex);
        void register_delete_callback(void* ptr, const std::function<void(Individual*)>& lambda);
        void unregister_delete_callback(void* ptr);
        
        Frame_t start_frame() const { return _startFrame; }
        Frame_t end_frame() const { return _endFrame; }
        size_t frame_count() const { return _basic_stuff.size(); }
        
        FrameRange get_recognition_segment(Frame_t frameIndex) const;
        FrameRange get_recognition_segment_safe(Frame_t frameIndex) const;
        FrameRange get_segment(Frame_t frameIndex) const;
        FrameRange get_segment_safe(Frame_t frameIndex) const;
        std::shared_ptr<SegmentInformation> segment_for(Frame_t frame) const;
        
        //! Returns iterator for the first segment equal to or before given frame
        decltype(_frame_segments)::const_iterator iterator_for(Frame_t frame) const;
        bool has(Frame_t frame) const;
        
        std::tuple<bool, FrameRange> frame_has_segment_recognition(Frame_t frameIndex) const;
        std::tuple<bool, FrameRange> has_processed_segment(Frame_t frameIndex) const;
        //const decltype(average_recognition_segment)::mapped_type& average_recognition(long_t segment_start) const;
        const decltype(average_recognition_segment)::mapped_type average_recognition(Frame_t segment_start);
        const decltype(average_recognition_segment)::mapped_type processed_recognition(Frame_t segment_start);
        std::tuple<size_t, Idx_t, float> average_recognition_identity(Frame_t segment_start) const;
        
        //! Properties based on centroid:
        const MotionRecord* centroid(Frame_t frameIndex) const;
        MotionRecord* centroid(Frame_t frameIndex);
        //! Properties based on posture / head position:
        const MotionRecord* head(Frame_t frameIndex) const;
        MotionRecord* head(Frame_t frameIndex);
        
        const MotionRecord* centroid_posture(Frame_t frameIndex) const;
        MotionRecord* centroid_posture(Frame_t frameIndex);
        
        const MotionRecord* centroid_weighted(Frame_t frameIndex) const;
        MotionRecord* centroid_weighted(Frame_t frameIndex);
        
        //! Raw blobs
        pv::BlobPtr blob(Frame_t frameIndex) const;
        pv::CompressedBlob* compressed_blob(Frame_t frameIndex) const;
        bool empty() const { return !_startFrame.valid(); }
        
        //const decltype(_training_data)& training_data() const { return _training_data; }
        //decltype(_training_data)& training_data() { return _training_data; }
        //void clear_training_data();
        
        //void save_posture(Frame_t frameIndex, Image::Ptr greyscale, Vec2 previous_direction);
        void save_posture(const BasicStuff& ptr, Frame_t frameIndex);
        Vec2 weighted_centroid(const Blob& blob, const std::vector<uchar>& pixels);
        
        long_t thresholded_size(Frame_t frameIndex) const;
        
        const Midline::Ptr midline(Frame_t frameIndex) const;
        //const Midline::Ptr cached_fixed_midline(Frame_t frameIndex);
        Midline::Ptr fixed_midline(Frame_t frameIndex) const;
        const Midline::Ptr pp_midline(Frame_t frameIndex) const;
        MinimalOutline::Ptr outline(Frame_t frameIndex) const;
        
        void iterate_frames(const Range<Frame_t>& segment, const std::function<bool(Frame_t frame, const std::shared_ptr<SegmentInformation>&, const Individual::BasicStuff*, const Individual::PostureStuff*)>& fn) const;
        
        BasicStuff* basic_stuff(Frame_t frameIndex) const;
        PostureStuff* posture_stuff(Frame_t frameIndex) const;
        std::tuple<BasicStuff*, PostureStuff*> all_stuff(Frame_t frameIndex) const;
        
        using Probability = Match::prob_t;
        struct DetailProbability {
            Match::prob_t p, p_time, p_pos, p_angle;
        };
        
        //! Calculates the probability for this fish to be at pixel-position in frame at time.
        Probability probability(int label, const IndividualCache& estimated_px, Frame_t frameIndex, const pv::BlobPtr& blob) const;
        Probability probability(int label, const IndividualCache& estimated_px, Frame_t frameIndex, const pv::CompressedBlob& blob) const;
        Probability probability(int label, const IndividualCache& estimated_px, Frame_t frameIndex, const Vec2& position, size_t pixels) const;
        Match::prob_t time_probability(const IndividualCache& cache, size_t recent_number_samples) const;
        //Match::PairingGraph::prob_t size_probability(const IndividualCache& cache, Frame_t frameIndex, size_t num_pixels) const;
        Match::prob_t position_probability(const IndividualCache& estimated_px, Frame_t frameIndex, size_t size, const Vec2& position, const Vec2& blob_center) const;
        
        const std::unique_ptr<BasicStuff>& find_frame(Frame_t frameIndex) const;
        bool evaluate_fitness() const;
        
        //void recognition_segment(Frame_t frame, const std::tuple<size_t, std::map<long_t, float>>&);
        void calculate_average_recognition();
        const decltype(_average_recognition)& average_recognition() const { return _average_recognition; }
        void clear_recognition();
        
        void add_manual_match(Frame_t frameIndex);
        void add_automatic_match(Frame_t frameIndex);
        bool is_manual_match(Frame_t frameIndex) const;
        bool is_automatic_match(Frame_t frameIndex) const;
        bool recently_manually_matched(Frame_t frameIndex) const;
        
        std::tuple<std::vector<std::tuple<float, float>>, std::vector<float>, size_t, MovementInformation> calculate_previous_vector(Frame_t frameIndex) const;
        
        /**
         * Calculates an actual cropped out image for a given frameIndex.
         * If something goes wrong it returns a nullptr.
         *
         * @param frameIndex the frame number within start_frame - end_frame
         * @param normalize If set to true, the function will normalize direction to be horizontal based on midline. If no midline is available it returns a nullptr
         * @param output_size If this parameter is non-empty, the returned image will be padded / cropped to the appropriate size with the fish in the center (according to midline length)
         * @param pixelized the blob saved inside the fish structure is most likely reduced, so it doesnt contain pixel information anymore. if this blob is provided, the pixels array from this blob will be used if needed.
         * @return an image pointer to a one-channel 8-bit greyscale image containing the difference image
         */
        
        static std::tuple<Image::UPtr, Vec2> calculate_diff_image(pv::BlobPtr blob, const Size2& output_size);
        static std::tuple<Image::UPtr, Vec2> calculate_normalized_diff_image(const gui::Transform& midline_transform, const pv::BlobPtr& blob, float midline_length, const Size2& output_size, bool use_legacy);
        static std::tuple<Image::UPtr, Vec2> calculate_normalized_image(const gui::Transform& midline_transform, const pv::BlobPtr& blob, float midline_length, const Size2& output_size, bool use_legacy);
        
        std::string toStr() const;
        static std::string class_name() {
            return "Individual";
        }
        
        //! Estimates the position in the given frame. Uses the previous position, returns
        //  position in the first frame if no previous position was available.
        //  Also pre-caches a few other properties of the individual.
        IndividualCache cache_for_frame(Frame_t frameIndex, double time, const CacheHints* = nullptr) const;
        
        void save_visual_field(const file::Path& path, Range<Frame_t> range = Range<Frame_t>({}, {}), const std::function<void(float, const std::string&)>& update = [](auto, auto){}, bool blocking = true);
        //size_t memory_size() const;
        
        static float weird_distance();
        //void push_to_segments(Frame_t frameIndex, long_t prev_frame);
        void clear_post_processing();
        void update_midlines(const CacheHints*);
        Midline::Ptr calculate_midline_for(const BasicStuff& basic, const PostureStuff& posture_stuff) const;
        
    private:
        friend class gui::Fish;
        
        std::shared_ptr<SegmentInformation> update_add_segment(Frame_t frameIndex, const MotionRecord& current, Frame_t prev_frame, const pv::CompressedBlob* blob, Match::prob_t current_prob);
        Midline::Ptr update_frame_with_posture(BasicStuff& basic, const decltype(Individual::_posture_stuff)::const_iterator& posture_it, const CacheHints* hints);
        //Vec2 add_current_velocity(Frame_t frameIndex, const MotionRecord* p);
    };
}

inline bool operator<(const std::shared_ptr<track::Individual::SegmentInformation>& ptr, cmn::Frame_t frame) {
    assert(ptr != nullptr);
    return ptr->start() < frame;
}

#endif
