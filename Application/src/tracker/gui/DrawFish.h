#ifndef _DRAW_FISH_H
#define _DRAW_FISH_H

#include <gui/types/Drawable.h>
#include <gui/types/Basic.h>
#include <gui/GuiTypes.h>
#include <gui/DrawStructure.h>
#include <tracking/Individual.h>
#include <tracking/Tracker.h>
#include <gui/Timeline.h>
#include <misc/EventAnalysis.h>
#include <gui/Graph.h>
#include <misc/OutputLibrary.h>

namespace pv {
struct CompressedBlob;
}

namespace gui {
    class Label;

    class Fish {
        Entangled _view;
        Label* _label { nullptr };

        track::Individual& _obj;
        const track::PPFrame* _frame;
        GETTER(Frame_t, idx)
        Frame_t _safe_idx;
        double _time;
        ExternalImage _image;
        int32_t _probability_radius = 0;
        Vec2 _probability_center;
        Midline::Ptr _cached_midline;
        MinimalOutline::Ptr _cached_outline;
        GETTER(Vec2, fish_pos)
        Circle _circle;

        std::vector<Vertex> _vertices;
        std::vector<std::unique_ptr<Vertices>> _paths;
        //Image _image;
        //Image *_probabilities;
        const EventAnalysis::EventMap* _events;
        
        Vec2 _position;
        float _plus_angle;
        ColorWheel _wheel;
        Color _color;
        Vec2 _v;
        std::shared_ptr<std::vector<Vec2>> _polygon_points;
        std::shared_ptr<Polygon> _polygon;
        
        Range<Frame_t> _prev_frame_range;
        
        struct FrameVertex {
            Frame_t frame;
            Vertex vertex;
            float speed_percentage;
        };
        
        std::deque<FrameVertex> frame_vertices;
        std::shared_ptr<Circle> _recognition_circle;
        std::vector<Vec2> points;
        
        pv::CompressedBlob *_blob;
        Bounds _blob_bounds;
        int _match_mode;
        IndividualCache _next_frame_cache;
        const Individual::BasicStuff* _basic_stuff{ nullptr };
        const Individual::PostureStuff* _posture_stuff{ nullptr };
        int _avg_cat = -1;
        Output::Library::LibInfo _info;
        double _library_y = Graph::invalid();
        //ExternalImage _colored;
        
        Graph _graph;
        Entangled _posture, _label_parent;
        
    public:
        Fish(track::Individual& obj);
        ~Fish();
        void update(Base* base, Drawable* bowl, Entangled& p, DrawStructure& d);
        //void draw_occlusion(DrawStructure& window);
        void set_data(Frame_t frameIndex, double time, const track::PPFrame& frame, const EventAnalysis::EventMap* events);
        
    private:
        //void paint(cv::Mat &target, int max_frames = 1000) const;
        void paintPath(const Vec2& offset, Frame_t to = {}, Frame_t from = {}, const Color& = Transparent);
        //void paintPixels() const;
        void update_recognition_circle();
    public:
        void label(Base*, Drawable* bowl, Entangled&);
        void shadow(DrawStructure&);
        void check_tags();
    };
}

#endif
