#include "InfoCard.h"
//#include <gui/gui.h>
#include <tracking/Tracker.h>
#include <tracking/Recognition.h>
#include <gui/Timeline.h>
#include <gui/types/Tooltip.h>
#include <gui/GUICache.h>
#include <gui/DrawBase.h>

namespace gui {
struct ShadowSegment {
    Range<Frame_t> frames;
    uint32_t error_code;
};
struct InfoCard::ShadowIndividual {
    Idx_t fdx{};
    track::Identity identity;
    pv::CompressedBlob blob;
    Frame_t frame{Frame_t::invalid};
    FrameRange current_range{};
    bool has_frame{false};
    bool is_automatic_match{false};
    float speed;
    
    FrameRange recognition_segment{};
    std::map<Idx_t, float> raw;
    std::string recognition_str;
    
    std::vector<ShadowSegment> segments, rec_segments;
};


InfoCard::InfoCard(std::function<void(Frame_t)> reanalyse)
    :
_shadow(new ShadowIndividual{}),
prev(std::make_shared<Button>("prev", Bounds(10, 0, 90, 25), Color(100, 100, 100, 200))),
next(std::make_shared<Button>("next", Bounds(105, 0, 90, 25), Color(100, 100, 100, 200))),
detail_button(std::make_shared<Button>("detail", Bounds(Vec2(), Size2(50,20)), Color(100, 100, 100, 200))),
_reanalyse(reanalyse)
{
}

InfoCard::~InfoCard() {
    delete _shadow;
}

void InfoCard::update() {
    static Tooltip tooltip(nullptr);
    Text *other = nullptr;
    
    for(auto &[text, tooltip_text] : segment_texts) {
        if(text->hovered()) {
            tooltip.set_text(tooltip_text);
            other = text;
            break;
        }
    }
    
    if(other != previous) {
        set_content_changed(true);
    }
    
    if(!content_changed())
        return;
    
    Color bg(50,50,50,125);
    
    auto &cache = GUICache::instance();
    if(!cache.has_selection() || !_shadow->fdx.valid()) {
        segment_texts.clear();
        other = nullptr;
        
        begin();
        end();
        return;
    }
    
    if(_shadow->fdx.valid()) {
        Tracker::LockGuard guard("InfoCard::update", 10);
        if(guard.locked()) {
            auto it = Tracker::individuals().find(_shadow->fdx);
            if(it != Tracker::individuals().end()) {
                auto fish = it->second;
                _shadow->identity = fish->identity();
                _shadow->has_frame = fish->has(_shadow->frame);
                _shadow->is_automatic_match = fish->is_automatic_match(_shadow->frame);
                
                auto basic = fish->basic_stuff(_shadow->frame);
                if(basic) {
                    _shadow->speed = basic->centroid.speed<Units::CM_AND_SECONDS>();
                    _shadow->blob = basic->blob;
                } else {
                    _shadow->speed = -1;
                    _shadow->blob = pv::CompressedBlob{};
                }
                
                auto blob_id = _shadow->blob.blob_id();
                auto && [valid, segment] = fish->has_processed_segment(_shadow->frame);
                
                std::string title = "recognition";
                
                if(valid) {
                    auto && [n, values] = fish->processed_recognition(segment.start());
                    title = "average n:"+Meta::toStr(n);
                    _shadow->raw = values;
                    
                } else {
                    _shadow->raw = Tracker::recognition()->ps_raw(_shadow->frame, blob_id);
                }
                
                _shadow->recognition_segment = segment;
                _shadow->recognition_str = title;
                
                auto range_of = [](const auto& rit) -> const FrameRange& {
                    using value_t = typename cmn::remove_cvref<decltype(rit)>::type;
                    using SegPtr_t = std::shared_ptr<Individual::SegmentInformation>;
                    
                    if constexpr(std::is_same<value_t, FrameRange>::value)    return rit;
                    else if constexpr(std::is_same<value_t, SegPtr_t>::value) return *rit;
                    else if constexpr(is_pair<value_t>::value) return rit.second;
                    else if constexpr(is_pair<typename cmn::remove_cvref<decltype(*rit)>::type>::value) return (*rit).second;
                    else if constexpr(std::is_same<decltype((*rit)->range), FrameRange>::value) return (*rit)->range;
                    else if constexpr(std::is_same<typename cmn::remove_cvref<decltype(*rit)>::type, std::shared_ptr<track::Individual::SegmentInformation>>::value)
                        return *(*rit);
                    else return *rit;
                };
                
                _shadow->segments.clear();
                _shadow->rec_segments.clear();
                
                for(auto it = fish->frame_segments().begin(); it != fish->frame_segments().end(); ++it)
                {
                    auto range = ((FrameRange)range_of(it)).range;
                    _shadow->segments.push_back(ShadowSegment{
                        range,
                        (*it)->error_code
                    });
                }
                
                for(auto it = fish->recognition_segments().begin(); it != fish->recognition_segments().end(); ++it)
                {
                    auto range = ((FrameRange)range_of(it)).range;
                    _shadow->rec_segments.push_back(ShadowSegment{
                        range,
                        0
                    });
                }
                
                FrameRange current_range;
                for(auto &s : _shadow->segments) {
                    if(s.frames.contains(_shadow->frame)) {
                        current_range = FrameRange{s.frames};
                        break;
                    }
                }
                _shadow->current_range = current_range;
                
            } else
                _shadow->fdx = Idx_t{};
            
        }
    }
    
    begin();
    
    auto clr = _shadow->identity.color();
    if(clr.r < 80) clr = clr + clr * ((80 - clr.r) / 80.f);
    else if(clr.g < 80) clr = clr + clr * ((80 - clr.g) / 80.f);
    else if(clr.b < 80) clr = clr + clr * ((80 - clr.b) / 80.f);
    
    //auto layout = std::make_shared<VerticalLayout>(Vec2(10, 10));
    add<Text>(_shadow->identity.name(), Vec2(11,11), White.alpha(clr.a * 0.7f), Font(0.9f, Style::Bold));
    auto text = add<Text>(_shadow->identity.name(), Vec2(10, 10), clr, Font(0.9f, Style::Bold));
    
    if(!_shadow->has_frame) {
        add<Text>(" (inactive)", text->pos() + Vec2(text->width(), 0), Gray.alpha(clr.a), Font(0.9f, Style::Bold));
    }
    
    auto &segments = _shadow->segments;
    segment_texts.clear();
    
    auto add_segments = [txt = text, this
#if DEBUG_ORIENTATION
                         ,fish
#endif
                         ](bool display_hints, const std::vector<ShadowSegment>& segments, float offx) {
        auto text = add<Text>(Meta::toStr(segments.size())+" segments", txt->pos() + Vec2(offx, Base::default_line_spacing(txt->font())), White, Font(0.8f));
        
#if DEBUG_ORIENTATION
        auto reason = fish->why_orientation(frameNr);
        std::string reason_str = "(none)";
        if(reason.frame == frameNr) {
            reason_str = reason.flipped_because_previous ? "previous_direction" : "(none)";
        }
        advance(new Text(reason_str, text->pos() + Vec2(0, Base::default_line_spacing(text->font())), White, Font(0.8)));
#endif
        
        // draw segment list
        auto rit = segments.rbegin();
        Frame_t current_segment;
        for(; rit != segments.rend(); ++rit) {
            if(rit->frames.end < _shadow->frame)
                break;
            
            current_segment = rit->frames.start;
            if(rit->frames.start <= _shadow->frame)
                break;
        }
        
        long_t i=0;
        while(rit != segments.rend() && ++rit != segments.rend() && ++i < 2);
        i = 0;
        auto it = rit == segments.rend()
            ? segments.begin()
            : track::find_frame_in_sorted_segments(segments.begin(), segments.end(), rit->frames.start);
        auto it0 = it;
        
        for (; it != segments.end() && cmn::abs(std::distance(it0, it)) < 5; ++it, ++i)
        {
            std::string str;
            auto range = it->frames;
            if(range.length() <= 1_f)
                str = range.start.toStr();
            else
                str = range.start.toStr() + "-" + range.end.toStr();
            
            //!TODO: Need to collect width() beforehand
            auto p = Vec2(width() - 10 + offx, float(height() - 40) * 0.5f + ((i - 2) + 1) * (float)Base::default_line_spacing(Font(1.1f)));
            auto alpha = 25 + 230 * (1 - cmn::abs(i-2) / 5.0f);
            
            text = add<Text>(str, p, _shadow->frame != range.start ? White.alpha(alpha) : Color(200,235,255).alpha(alpha), Font(0.8f), Vec2(1), Vec2(1, 0.5f));
            text->set_clickable(true);
            //text = advance(text);
            
            std::string tt;
            if(display_hints) {
                const ShadowSegment& ptr = *it;
                auto bitset = ptr.error_code;
                if(ptr.error_code != std::numeric_limits<decltype(ptr.error_code)>::max()) {
                    size_t i=0;
                    while (bitset != 0) {
                        auto t = bitset & -bitset;
                        int r = __builtin_ctz(bitset);
                        if(size_t(r + 1) >= ReasonsNames.size())
                            tt += std::string(i > 0 ? "," : "")+" <key>invalid-key</key>";
                        else
                            tt += std::string(i > 0 ? "," : "")+" <str>"+std::string(ReasonsNames.at(r + 1))+"</str>";
                        //reasons.push_back((Reasons)(1 << r));
                        bitset ^= t;
                        ++i;
                    }
                } else {
                    tt += " <nr>Analysis ended</nr>";
                }
                
                tt = "Segment "+Meta::toStr(ptr.frames)+" ended because:"+tt;
            }
            segment_texts.push_back({text, tt});
            
            if(it->frames.start == current_segment) {
                bool inside = it->frames.contains(_shadow->frame);
                auto offy = - (inside ? 0.f : (Base::default_line_spacing(Font(1.1f))*0.5f));
                add<Line>(Vec2(10+offx, p.y + offy), Vec2(text->pos().x - (!inside ? 0 : text->width() + 10), p.y + offy), inside ? White : White.alpha(100));
            }
        }
    };
    
    add_segments(true, _shadow->segments, 0);
    add_segments(false, _shadow->rec_segments, 200);
    
    static bool first = true;
    
    if(first) {
        prev->on_click([](auto) {
            auto & cache = GUICache::instance();
            auto next_frame = cache.frame_idx;
            if(cache.has_selection()) {
                Tracker::LockGuard guard("InfoCard::update->prev->on_click");
                auto segment = cache.primary_selection()->get_segment(next_frame);
                
                if(next_frame == segment.start())
                    next_frame = cache.primary_selection()->get_segment(segment.start() - 1_f).start();
                else
                    next_frame = segment.start();
            }
            
            if(!next_frame.valid())
                return;
            
            if(cache.frame_idx != next_frame)
                SETTING(gui_frame) = Frame_t(next_frame);
        });
        
        next->on_click([](auto) {
            auto & cache = GUICache::instance();
            auto next_frame = cache.frame_idx;
            if(cache.has_selection()) {
                Tracker::LockGuard guard("InfoCard::update->next->on_click");
                auto segment = cache.primary_selection()->get_segment(next_frame);
                if(segment.start().valid()) {
                    auto it = cache.primary_selection()->find_segment_with_start(segment.start());
                    ++it;
                    if(it == cache.primary_selection()->frame_segments().end()) {
                        next_frame.invalidate();
                    } else {
                        next_frame = (*it)->start();
                    }
                    
                } else
                    next_frame.invalidate();
            }
            
            if(!next_frame.valid())
                return;
            
            if(cache.frame_idx != next_frame)
                SETTING(gui_frame) = next_frame;
        });
        
        first = false;
    }
    
    next->set_pos(Vec2(next->pos().x, height() - next->height() - 10));
    prev->set_pos(Vec2(10, next->pos().y));
    
    advance_wrap(*next);
    advance_wrap(*prev);
    
    float y = Base::default_line_spacing(Font(1.1f)) * 8 + 40;
    bool fish_has_frame = _shadow->has_frame;
    if(!fish_has_frame)
        bg = Color(100, 100, 100, 125);
    
    auto fdx = _shadow->fdx;
    auto fprobs = cache.probs(fdx);
    
    bool detail = SETTING(gui_show_detailed_probabilities);
    Bounds tmp(0, y - 10, 200, 0);
    
    //auto idx = index();
    //if(idx < children().size() && children().at(idx)->type() == Type::RECT)
    //    tmp << children().at(idx)->size();
    
    
    
    float max_w = 200;
    
    
    auto rect = add<Rect>(tmp, bg.alpha(detail ? 50 : bg.a));
    text = add<Text>("matching", Vec2(10, y), White, Font(0.8f, Style::Bold));
    
    /*if(!detail_button->parent()) {
        detail_button->set_toggleable(true);
        detail_button->set_toggle(SETTING(gui_show_detailed_probabilities));
        detail_button->clear_event_handlers();
        detail_button->on_click([](auto) {
            if(GUI::instance())
                SETTING(gui_show_detailed_probabilities) = !SETTING(gui_show_detailed_probabilities);
        });
    }
    
    detail_button->set_pos(Vec2(text->width() + text->pos().x + 15, y + (text->height() - detail_button->height()) * 0.5f));
    advance_wrap(*detail_button);
    
    if(detail_button->pos().x + detail_button->width() + 10 > max_w)
        max_w = detail_button->pos().x + detail_button->width() + 10;*/
    
    if(_shadow->is_automatic_match) {
        y += text->height();
        text = add<Text>("(automatic match)", Vec2(10, y), White.alpha(150), Font(0.8f, Style::Italic));
        y += text->height();
        
        if(!automatic_button) {
            automatic_button = std::make_shared<Button>("delete", Bounds(10, y, 50, 20), Color(100, 200));
            automatic_button->on_click([this](auto){
                if(!_shadow->fdx.valid())
                    return;
                
                Tracker::LockGuard guard("InfoCard::update->delete->on_click");
                if(!_shadow->current_range.empty()) {
                    print("Erasing automatic matches for fish ", _shadow->fdx," in range ", _shadow->current_range.start(),"-",_shadow->current_range.end());
                    Tracker::delete_automatic_assignments(_shadow->fdx, _shadow->current_range);
                    _reanalyse(_shadow->frame);
                }
            });
        }
        
        advance_wrap(*automatic_button);
        y += automatic_button->height();
        
        if(text->width() + text->pos().x + 10 > max_w)
            max_w = text->width() + text->pos().x + 10;
        
    } else
        y += text->height();
    
    std::string speed_str = _shadow->speed < 0 ? "(none)" : (Meta::toStr(_shadow->speed) + "cm/s");
    
    y += add<Text>(speed_str, Vec2(10, y), White.alpha(125), Font(0.8f))->height();
    /*if (!_shadow->current_range.empty()) {
        auto [id, p] = _fish->qrcode_at(_shadow->current_range->start());
        if (id != -1) {
            y += add<Text>("QR:" + Meta::toStr(id) + " (" + Meta::toStr(p) + ")", Vec2(10, y), White.alpha(125), Font(0.8))->height();
        }
    }*/
    
    if(fprobs) {
        track::Match::prob_t max_prob = 0;
        pv::bid bdx;
        for(auto &blob : cache.processed_frame.blobs()) {
            if(fprobs->count(blob->blob_id())) {
                auto &probs = (*fprobs).at(blob->blob_id());
                if(probs/*.p*/ > max_prob) {
                    max_prob = probs/*.p*/;
                    bdx = blob->blob_id();
                }
            }
        }
        
        for(auto &blob : cache.processed_frame.blobs()) {
            if(fprobs->count(blob->blob_id())) {
                auto color = Color(200, 200, 200, 255);
                if(cache.fish_selected_blobs.find(fdx) != cache.fish_selected_blobs.end() && blob->blob_id() == cache.fish_selected_blobs.at(fdx)) {
                    color = Green;
                } else if(blob->blob_id() == bdx) {
                    color = Yellow;
                }
                
                auto &probs = (*fprobs).at(blob->blob_id());
                auto probs_str = Meta::toStr(probs/*.p*/);
                /*if(detail)
                    probs_str += " (p:"+Meta::toStr(probs.p_pos)+" a:"+Meta::toStr(probs.p_angle)+" s:"+Meta::toStr(probs.p_pos / probs.p_angle)+" t:"+Meta::toStr(probs.p_time)+")";*/
                
                auto text = add<Text>(Meta::toStr(blob->blob_id())+": ", Vec2(10, y), White, Font(0.8f));
                auto second = add<Text>(probs_str, text->pos() + Vec2(text->width(), 0), color, Font(0.8f));
                y += text->height();
                
                auto w = second->pos().x + second->width() + 10;
                if(w > max_w)
                    max_w = w;
            }
        }
    }
        
    tmp.width = max_w;
    tmp.height = y - tmp.y + 10;
    
    rect->set_size(tmp.size());
    
    y += 30;
    
    if(Recognition::recognition_enabled() && fish_has_frame) {
        Bounds tmp(0, y-10, 200, 0);
        auto rect = add<Rect>(tmp, bg.alpha(bg.a));
        
        //auto idx = index();
        //if(idx < children().size() && children().at(idx)->type() == Type::RECT)
        //    tmp << children().at(idx)->size();
        
        float p_sum = 0;
        for(auto && [key, value] : _shadow->raw)
            p_sum = max(p_sum, value);
        
        float max_w = 200;
        auto text = add<Text>(_shadow->recognition_str, Vec2(10, y), White, Font(0.8f, Style::Bold));
        y += text->height();
        max_w = max(max_w, 10 + text->width() + text->pos().x);
        
        text = add<Text>(Meta::toStr(_shadow->recognition_segment), Vec2(10, y), Color(220,220,220,255), Font(0.8f, Style::Italic));
        y += text->height();
        max_w = max(max_w, 10 + text->width() + text->pos().x);
        
        if(!_shadow->raw.empty())
            y += 5;
        
        long_t mdx = -1;
        float mdx_p = 0;
        for(auto&& [fdx, p] : _shadow->raw) {
            if(p > mdx_p) {
                mdx_p = p;
                mdx = fdx;
            }
        }
        
        Vec2 current_pos(10, y);
        float _max_y = y;
        float _max_w = 0;
        size_t column_count = 0;
        
        for(auto [fdx, p] : _shadow->raw) {
            p *= 100;
            p = roundf(p);
            p /= 100;
            
            std::string str = Meta::toStr(fdx) + ": " + Meta::toStr(p);
            
            Color color = White * (1 - p/p_sum) + Red * (p / p_sum);
            auto text = add<Text>(str, current_pos, color, Font(0.8f));
            
            auto w = text->pos().x + text->width() + 10;
            if(w > max_w)
                max_w = w;
            if(w > _max_w)
                _max_w = w;
            
            current_pos.y += text->height();
            ++column_count;
            
            if(current_pos.y > _max_y)
                _max_y = current_pos.y;
            
            if(column_count > 25) {
                column_count = 0;
                current_pos.y = y;
                current_pos.x = _max_w;
            }
        }
        
        tmp.width = max_w;
        tmp.height = _max_y - tmp.y + 10;
        
        rect->set_size(tmp.size());
    }
    
    if(other) {
        tooltip.set_other(other);
        advance_wrap(tooltip);
    }
    
    end();
    
    set_background(bg);
}
    
    void InfoCard::update(gui::DrawStructure &base, Frame_t frameNr) {
        auto fish = GUICache::instance().primary_selection();
        
        if(fish) {
            if(_shadow->fdx != fish->identity().ID()) {
                segment_texts.clear();
                previous = nullptr;
                
                /*if(_fish) {
                    _fish->unregister_delete_callback(this);
                }
                
                fish->register_delete_callback(this, [this](Individual*) {
                    if(!GUI::instance())
                        return;
                    std::lock_guard<std::recursive_mutex> guard(GUI::instance()->gui().lock());
                    _shadow->fdx = Idx_t{};
                    set_content_changed(true);
                });*/
            }
            
            if(_shadow->frame != frameNr || _shadow->fdx != fish->identity().ID())
            {
                set_content_changed(true);
                
                _shadow->frame = frameNr;
                _shadow->fdx = fish->identity().ID();
            }
            
        } else {
            _shadow->fdx = Idx_t{};
        }
        
        set_origin(Vec2(0, 0));
        set_bounds(Bounds((10) / base.scale().x, 100 / base.scale().y, 200, Base::default_line_spacing(Font(1.1f)) * 7 + 60));
        set_scale(base.scale().reciprocal());
    }
}
