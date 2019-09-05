/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#include "perfetto/tracing.h"
//
//#include "perfetto/common/gpu_counter_descriptor.pbzero.h"
//#include "perfetto/config/gpu/gpu_counter_config.pbzero.h"
//#include "perfetto/protozero/scattered_heap_buffer.h"
//#include "perfetto/trace/gpu/gpu_counter_event.pbzero.h"
//#include "perfetto/trace/gpu/gpu_render_stage_event.pbzero.h"
//#include "perfetto/trace/test_event.pbzero.h"
//#include "perfetto/trace/trace.pb.h"
//#include "perfetto/trace/trace_packet.pbzero.h"
#include "perfetto.h"

#include <unordered_map>
#include <vector>
#include <math.h>
#include <android/log.h>

// Deliberately not pulling any non-public perfetto header to spot accidental
// header public -> non-public dependency while building this file.

class GpuCounterDataSource : public perfetto::DataSource<GpuCounterDataSource> {
 public:
  void OnSetup(const SetupArgs& args) override {
    // This can be used to access the domain-specific DataSourceConfig, via
    // args.config->xxx_config_raw().
    PERFETTO_ILOG("GpuCounterDataSource OnSetup, name: %s", args.config->name().c_str());
    const std::string& config_raw = args.config->gpu_counter_config_raw();
    perfetto::protos::pbzero::GpuCounterConfig::Decoder config(config_raw);
    for(auto it = config.counter_ids(); it; ++it) {
      counter_ids.push_back(it->as_uint32());
    }
    first = true;
  }

  void OnStart(const StartArgs&) override { PERFETTO_ILOG("GpuCounterDataSource OnStart called"); }

  void OnStop(const StopArgs&) override { PERFETTO_ILOG("GpuCounterDataSource OnStop called"); }

  bool first = true;
  uint64_t count = 0;
  std::vector<uint32_t> counter_ids;
};

class GpuRenderStageDataSource : public perfetto::DataSource<GpuRenderStageDataSource> {
 public:
  void OnSetup(const SetupArgs& args) override {
    PERFETTO_ILOG("GpuRenderStageDataSource OnSetup called, name: %s", args.config->name().c_str());
    first = true;
  }

  void OnStart(const StartArgs&) override { PERFETTO_ILOG("GpuRenderStageDataSource OnStart called"); }

  void OnStop(const StopArgs&) override { PERFETTO_ILOG("GpuRenderStageDataSource OnStop called"); }

  bool first = true;
  uint64_t count = 0;
};

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(GpuCounterDataSource);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(GpuRenderStageDataSource);

extern "C" {
int startProducer() {
  const std::unordered_map<uint32_t, const char*> COUNTER_MAP {
    { 0, "foo" },
    { 1, "bar" }
  };

  perfetto::TracingInitArgs args;
  args.backends = perfetto::kSystemBackend;
  perfetto::Tracing::Initialize(args);

  // DataSourceDescriptor can be used to advertise domain-specific features.
  {
    perfetto::DataSourceDescriptor dsd;
    dsd.set_name("gpu.counters");

    protozero::HeapBuffered<perfetto::protos::pbzero::GpuCounterDescriptor> proto;
    for (auto it : COUNTER_MAP) {
    auto spec = proto->add_specs();
      spec->set_counter_id(it.first);
      spec->set_name(it.second);
    }
    dsd.set_gpu_counter_descriptor_raw(proto.SerializeAsString());
    GpuCounterDataSource::Register(dsd);
  }

  {
    perfetto::DataSourceDescriptor dsd;
    dsd.set_name("gpu.renderingstages");
    GpuRenderStageDataSource::Register(dsd);
  }

  for (;;) {
    GpuCounterDataSource::Trace([&](GpuCounterDataSource::TraceContext ctx) {
      PERFETTO_LOG("GpuCounterDataSource tracing lambda called");
      auto data_source = ctx.GetDataSourceLocked();
      if (data_source->first) {
        data_source->count = 0;
        auto packet = ctx.NewTracePacket();
        packet->set_timestamp(0);
        auto counter_event = packet->set_gpu_counter_event();
        auto desc = counter_event->set_counter_descriptor();
        for (auto it : data_source->counter_ids) {
          auto entry = COUNTER_MAP.find(it);
          if (entry != COUNTER_MAP.end()) {
            auto spec = desc->add_specs();
            spec->set_counter_id(entry->first);
            spec->set_name(entry->second);
          }
        }
        packet->Finalize();
        data_source->first = false;
      }
      data_source->count++;
      {
        int cnt = data_source->count;
        auto packet = ctx.NewTracePacket();
        packet->set_timestamp(cnt * 10);
        auto counter_event = packet->set_gpu_counter_event();
        auto counters = counter_event->add_counters();
        counters->set_counter_id(cnt % 3);
        if (cnt % 3 == 0) {
          counters->set_double_value(static_cast<double>(cnt));
        } else {
          counters->set_int_value(static_cast<int64_t>(cnt));
        }
        packet->Finalize();
      }
    });

    GpuRenderStageDataSource::Trace([&](GpuRenderStageDataSource::TraceContext ctx) {
      PERFETTO_LOG("GpuRenderStageDataSource tracing lambda called");
      __android_log_print(ANDROID_LOG_INFO, "TAG", "Adithya - Inside lambda RenderStageDataSource");
      auto data_source = ctx.GetDataSourceLocked();
      if (data_source->first) {
        data_source->count = 0;
        auto packet = ctx.NewTracePacket();
        packet->set_timestamp(0);
        auto event = packet->set_gpu_render_stage_event();
        auto spec = event->set_specifications();
        auto hw_queue = spec->add_hw_queue();
        hw_queue->set_name("queue 0");
        hw_queue = spec->add_hw_queue();
        hw_queue->set_name("queue 1");
        auto stage = spec->add_stage();
        stage->set_name("stage 0");
        stage = spec->add_stage();
        stage->set_name("stage 1");
        stage = spec->add_stage();
        stage->set_name("stage 2");
        packet->Finalize();
        data_source->first = false;
      }
      data_source->count++;
      {
        int cnt = data_source->count;
        auto packet = ctx.NewTracePacket();
        packet->set_timestamp(cnt * 10);
        auto event = packet->set_gpu_render_stage_event();
        event->set_event_id(cnt);
        event->set_duration(5);
        event->set_hw_queue_id(cnt % 2);
        event->set_stage_id(cnt % 3);
        event->set_context(42);
        if (cnt % 4) {
            auto* ext0 = event->add_extra_data();
            ext0->set_name("keyOnlyTest");
            if (cnt % 2) {
                auto* ext1 = event->add_extra_data();
                ext1->set_name("stencilBPP");
                ext1->set_value("1");
            }
            auto* ext2 = event->add_extra_data();
            ext2->set_name("height");
            ext2->set_value(std::to_string(pow(cnt, 2)).c_str());
        }
        packet->Finalize();
      }
    });
    sleep(1);
  }
  return 0;
}
}
