#pragma once
#if WITH_FFMPEG

#include <commons.pc.h>
#include <types.h>
#include <misc/Image.h>
#include <file/Path.h>
#include <lzo/minilzo.h>
#include "gpuImage.h"

class FFMPEGQueue {
    std::mutex _mutex, _write_mutex;
    std::vector<std::unique_ptr<cmn::Image>> _queue;
    std::condition_variable _condition, _write_condition;
    cmn::Size2 _size;
    cv::Mat _image;
    file::Path _output_path;
    long pts;
    
    struct Package {
        uchar *memory;
        lzo_uint in_len, out_len;
        cmn::timestamp_t timestamp;
        
        Package() {}
        
        ~Package() {
            free(memory);
        }
        
        void unpack(cmn::Image& image, lzo_uint& new_len) const;
    };
    
    cmn::timestamp_t _last_timestamp;
    GETTER_NCONST(std::atomic_bool, terminate)
    std::vector<cmn::timestamp_t> timestamps;
    std::vector<long> mp4_indexes;
    std::deque<std::shared_ptr<Package>> packages;
    
    //std::mutex _vacant_mutex;
    //std::deque<cmn::Image*> _vacant_images;
    bool _direct;
    std::thread *write_thread;
    
    double average_compressed_size, samples_compressed_size;
    
public:
    FFMPEGQueue(bool direct, const cmn::Size2& size, const file::Path& output);
    ~FFMPEGQueue();
    
    void loop();
    void write_loop();
    void notify();
    void add(std::unique_ptr<cmn::Image>&& ptr);
    
    //void refill_queue(std::queue<std::unique_ptr<cmn::Image_t>>& queue);
    
private:
    void process_one_image(cmn::timestamp_t stamp, const std::unique_ptr<cmn::Image>& ptr, bool direct);
    void finalize_one_image(cmn::timestamp_t stamp, const cmn::Image& image);
    void update_cache_strategy(double frame_ms, double compressed_size);
    
    void open_video();
    void close_video();
    void update_statistics(double ms, double image_size);
};

#endif
