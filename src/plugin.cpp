/*
    MIT License

    Copyright (c) 2022 HolyWu
    Copyright (c) 2022 Asd-g

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <algorithm>
#include <atomic>
#include <fstream>
#include <memory>
#include <semaphore>
#include <string>
#include <vector>

#include "avisynth_c.h"
#include "boost/dll/runtime_symbol_info.hpp"
#include "realcugan.h"

using namespace std::literals;

static std::atomic<int> numGPUInstances{ 0 };

struct realcugan
{
    AVS_FilterInfo* fi;
    std::unique_ptr<RealCUGAN> realcugan; //!
    std::unique_ptr<std::counting_semaphore<>> semaphore;
    std::string msg;
};

static void filter(const AVS_VideoFrame* src, AVS_VideoFrame* dst, const realcugan* const __restrict d) noexcept
{
    const auto c { avs_num_components(&d->fi->vi) };
    const auto width{ avs_get_row_size_p(src, AVS_DEFAULT_PLANE) / c };
    const auto height{ avs_get_height_p(src, AVS_DEFAULT_PLANE) };

    d->semaphore->acquire();
    const ncnn::Mat inimage = ncnn::Mat(width, height, (void*)avs_get_read_ptr_p(src, AVS_DEFAULT_PLANE), (size_t)c, c);
    ncnn::Mat outimage = ncnn::Mat(width * d->realcugan->scale, height * d->realcugan->scale, (void*)avs_get_write_ptr_p(dst, AVS_DEFAULT_PLANE), (size_t)c, c);
    d->realcugan->process(inimage, outimage); //!(srcR, srcG, srcB, dstR, dstG, dstB, width, height, srcStride, dstStride);
    d->semaphore->release();
}

static AVS_VideoFrame* AVSC_CC realcugan_get_frame(AVS_FilterInfo* fi, int n)
{
    realcugan* d{ static_cast<realcugan*>(fi->user_data) };
    auto src{ avs_get_frame(fi->child, n) };
    if (!src)
        return nullptr;

    auto dst{ avs_new_video_frame_p(fi->env, &fi->vi, src) };

    filter(src, dst, d);

    avs_release_video_frame(src);

    return dst;
}

static void AVSC_CC free_realcugan(AVS_FilterInfo* fi)
{
    auto d{ static_cast<realcugan*>(fi->user_data) };
    delete d;

    if (--numGPUInstances == 0)
        ncnn::destroy_gpu_instance();
}

static int AVSC_CC realcugan_set_cache_hints(AVS_FilterInfo* fi, int cachehints, int frame_range)
{
    return cachehints == AVS_CACHE_GET_MTMODE ? 2 : 0;
}

static AVS_Value AVSC_CC Create_realcugan(AVS_ScriptEnvironment* env, AVS_Value args, void* param)
{
    enum { Clip, Noise, Scale, Tilesize, Model, Gpu_id, Gpu_thread, Tta, List_gpu };

    auto d{ new realcugan() };

    AVS_Clip* clip{ avs_new_c_filter(env, &d->fi, avs_array_elt(args, Clip), 1) };
    AVS_Value v{ avs_void };

    try {
        if (!avs_check_version(env, 9))
        {
            if (avs_check_version(env, 10))
            {
                if (avs_get_env_property(env, AVS_AEP_INTERFACE_BUGFIX) < 2)
                    throw "AviSynth+ version must be r3688 or later.";
            }
        }
        else
            throw "AviSynth+ version must be r3688 or later.";

        if (avs_is_planar(&d->fi->vi) ||
            !avs_is_rgb(&d->fi->vi) ||
            avs_component_size(&d->fi->vi) != 1)
            throw "only 8-bit RGB formats supported";

        if (ncnn::create_gpu_instance())
            throw "failed to create GPU instance";
        ++numGPUInstances;

        const auto noise{ avs_defined(avs_array_elt(args, Noise)) ? avs_as_int(avs_array_elt(args, Noise)) : 0 };
        const auto scale{ avs_defined(avs_array_elt(args, Scale)) ? avs_as_int(avs_array_elt(args, Scale)) : 2 };
        const auto tilesize{ avs_defined(avs_array_elt(args, Tilesize)) ? avs_as_int(avs_array_elt(args, Tilesize)) : (std::max)(d->fi->vi.width, 32) };
        const auto model{ avs_defined(avs_array_elt(args, Model)) ? avs_as_int(avs_array_elt(args, Model)) : 2 };
        const auto gpuId{ avs_defined(avs_array_elt(args, Gpu_id)) ? avs_as_int(avs_array_elt(args, Gpu_id)) : ncnn::get_default_gpu_index() };
        const auto gpuThread{ avs_defined(avs_array_elt(args, Gpu_thread)) ? avs_as_int(avs_array_elt(args, Gpu_thread)) : 2 };
        const auto tta{ avs_defined(avs_array_elt(args, Tta)) ? avs_as_bool(avs_array_elt(args, Tta)) : 0 };

        if (noise < -1 || noise > 3)
            throw "noise must be between -1 and 3 (inclusive)";
        if (scale < 2 || scale > 4)
            throw "scale must be between 2 and 4 (inclusive)";
        if (tilesize < 32)
            throw "tilesize must be at least 32";
        if (model < 0 || model > 2)
            throw "model must be between 0 and 2 (inclusive)";
        if (gpuId < 0 || gpuId >= ncnn::get_gpu_count())
            throw "invalid GPU device";
        if (auto queue_count{ ncnn::get_gpu_info(gpuId).compute_queue_count() }; gpuThread < 1 || static_cast<uint32_t>(gpuThread) > queue_count)
            throw ("gpu_thread must be between 1 and " + std::to_string(queue_count) + " (inclusive)").c_str();

        if (avs_defined(avs_array_elt(args, List_gpu)) ? avs_as_bool(avs_array_elt(args, List_gpu)) : 0)
        {
            for (auto i{ 0 }; i < ncnn::get_gpu_count(); ++i)
                d->msg += std::to_string(i) + ": " + ncnn::get_gpu_info(i).device_name() + "\n";

            AVS_Value cl{ avs_new_value_clip(clip) };
            AVS_Value args_[2]{ cl , avs_new_value_string(d->msg.c_str()) };
            v = avs_invoke(d->fi->env, "Text", avs_new_value_array(args_, 2), 0);

            avs_release_value(cl);
            avs_release_clip(clip);

            if (--numGPUInstances == 0)
                ncnn::destroy_gpu_instance();

            return v;
        }

        if (noise == -1 && scale == 1)
        {
            v = avs_new_value_clip(clip);

            avs_release_clip(clip);

            if (--numGPUInstances == 0)
                ncnn::destroy_gpu_instance();

            return v;
        }

        d->fi->vi.width *= scale;
        d->fi->vi.height *= scale;

        auto modelDir{ boost::dll::this_line_location().parent_path().generic_string() + "/models" };
        int prepadding{};

        switch (model)
        {
            case 0:
            {
                modelDir += "/models-nose";
                break;
            }
            case 1:
            {
                modelDir += "/models-pro";
                break;
            }
            case 2:
            {
                modelDir += "/models-se";
                break;
            }
        }

        if (scale == 2)
        {
            prepadding = 18;
        }
        if (scale == 3)
        {
            prepadding = 14;
        }
        if (scale == 4)
        {
            prepadding = 19;
        }

        std::string paramPath, modelPath;

        if (noise == -1)
        {
            paramPath = modelDir + "/up" + std::to_string(scale) + "x-conservative.param";
            modelPath = modelDir + "/up" + std::to_string(scale) + "x-conservative.bin";
        }
        else if (noise == 0)
        {
            paramPath = modelDir + "/up" + std::to_string(scale) + "x-no-denoise.param";
            modelPath = modelDir + "/up" + std::to_string(scale) + "x-no-denoise.bin";
        }
        else
        {
            paramPath = modelDir + "/up" + std::to_string(scale) + "x-denoise" + std::to_string(noise) + "x.param";
            modelPath = modelDir + "/up" + std::to_string(scale) + "x-denoise" + std::to_string(noise) + "x.bin";
        }

        std::ifstream ifs{ paramPath };
        if (!ifs.is_open())
            throw "failed to load model";
        ifs.close();

        d->realcugan = std::make_unique<RealCUGAN>(gpuId, tta, 1);

#ifdef _WIN32
        const auto paramBufferSize{ MultiByteToWideChar(CP_UTF8, 0, paramPath.c_str(), -1, nullptr, 0) };
        const auto modelBufferSize{ MultiByteToWideChar(CP_UTF8, 0, modelPath.c_str(), -1, nullptr, 0) };
        std::vector<wchar_t> wparamPath(paramBufferSize);
        std::vector<wchar_t> wmodelPath(modelBufferSize);
        MultiByteToWideChar(CP_UTF8, 0, paramPath.c_str(), -1, wparamPath.data(), paramBufferSize);
        MultiByteToWideChar(CP_UTF8, 0, modelPath.c_str(), -1, wmodelPath.data(), modelBufferSize);
        d->realcugan->load(wparamPath.data(), wmodelPath.data()); //! , fp32);
#else
        d->realcugan->load(paramPath, modelPath, fp32);
#endif

        d->realcugan->noise = noise;
        d->realcugan->scale = scale;
        d->realcugan->tilesize = tilesize;
        d->realcugan->prepadding = prepadding;

        d->semaphore = std::make_unique<std::counting_semaphore<>>(gpuThread);
    }
    catch (const char* error)
    {
        d->msg = "realcugan_nvk: "s + error;
        v = avs_new_value_error(d->msg.c_str());

        if (--numGPUInstances == 0)
            ncnn::destroy_gpu_instance();
    }

    if (!avs_defined(v))
    {
        v = avs_new_value_clip(clip);

        d->fi->user_data = reinterpret_cast<void*>(d);
        d->fi->get_frame = realcugan_get_frame;
        d->fi->set_cache_hints = realcugan_set_cache_hints;
        d->fi->free_filter = free_realcugan;
    }

    avs_release_clip(clip);

    return v;
    }

const char* AVSC_CC avisynth_c_plugin_init(AVS_ScriptEnvironment* env)
{
    avs_add_function(env, "realcugan", "c[noise]i[scale]i[tilesize]i[model]i[gpu_id]i[gpu_thread]i[tta]b[list_gpu]b", Create_realcugan, 0);
    return "realcugan ncnn Vulkan";
}
