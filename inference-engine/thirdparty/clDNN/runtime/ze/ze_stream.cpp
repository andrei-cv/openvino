// Copyright (C) 2019-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "ze_stream.hpp"
#include "ze_event.hpp"
#include "ze_kernel.hpp"
#include "ze_common.hpp"

#include <cassert>
#include <iomanip>
#include <ios>

#include <fstream>
#include <thread>
#include <string>
#include <vector>
#include <memory>

namespace cldnn {
namespace ze {

namespace {
inline ze_group_count_t to_group_count(const std::vector<size_t>& v) {
     switch (v.size()) {
        case 1:
            return {uint32_t(v[0]), uint32_t(1), uint32_t(1)};
        case 2:
            return {uint32_t(v[0]), uint32_t(v[1]), uint32_t(1)};
        case 3:
            return {uint32_t(v[0]), uint32_t(v[1]), uint32_t(v[2])};
        default:
            return {uint32_t(0), uint32_t(0), uint32_t(0)};
    }
}

void set_arguments_impl(ze_kernel_handle_t kernel,
                         const arguments_desc& args,
                         const kernel_arguments_data& data) {
    using args_t = argument_desc::Types;
    using scalar_t = scalar_desc::Types;

    for (uint32_t i = 0; i < static_cast<uint32_t>(args.size()); i++) {
        ze_result_t status = ZE_RESULT_NOT_READY;
        switch (args[i].t) {
            case args_t::INPUT:
                if (args[i].index < data.inputs.size() && data.inputs[args[i].index]) {
                //if (args[i].index < data.inputs.size()) {
                    const auto& input_mem = data.inputs[args[i].index];
                    if (input_mem) {
                        if (memory_capabilities::is_usm_type(input_mem->get_allocation_type())) {
                            //status = kernel.setArgUsm(i, std::dynamic_pointer_cast<const ze::gpu_usm>(input_mem)->get_buffer());
                            auto ptr = std::dynamic_pointer_cast<const ze::gpu_usm>(input_mem)->get_buffer().get();
                            status = zeKernelSetArgumentValue(kernel, i, sizeof(void*), &ptr);
                            ZE_CHECK(status);
                        }
                    }
                }
                break;
            case args_t::INPUT_OF_FUSED_PRIMITIVE:
                if (args[i].index < data.fused_op_inputs.size() && data.fused_op_inputs[args[i].index]) {
                    const auto& input_mem = data.fused_op_inputs[args[i].index];
                    if (input_mem) {
                        if (memory_capabilities::is_usm_type(input_mem->get_allocation_type())) {
                            //status = kernel.setArgUsm(i, std::dynamic_pointer_cast<const ze::gpu_usm>(input_mem)->get_buffer());
                            auto ptr = std::dynamic_pointer_cast<const ze::gpu_usm>(input_mem)->get_buffer().get();
                            status = zeKernelSetArgumentValue(kernel, i, sizeof(void*), &ptr);
                            ZE_CHECK(status);
                        }
                    }
                }
                break;
            case args_t::INTERNAL_BUFFER:
                if (args[i].index < data.intermediates.size() && data.intermediates[args[i].index]) {
                    const auto& input_mem = data.intermediates[args[i].index];
                    if (input_mem) {
                        if (memory_capabilities::is_usm_type(input_mem->get_allocation_type())) {
                            auto ptr = std::dynamic_pointer_cast<const ze::gpu_usm>(input_mem)->get_buffer().get();
                            status = zeKernelSetArgumentValue(kernel, i, sizeof(void*), &ptr);
                            ZE_CHECK(status);
                        }
                    }
                }
                break;
            case args_t::OUTPUT:
                if (data.output) {
                    auto ptr = std::dynamic_pointer_cast<const ze::gpu_usm>(data.output)->get_buffer().get();
                    status = zeKernelSetArgumentValue(kernel, i, sizeof(void*), &ptr);
                    ZE_CHECK(status);
                }
                break;
            case args_t::WEIGHTS:
                if (data.weights) {
                    //status = kernel.setArgUsm(i, std::dynamic_pointer_cast<const ze::gpu_usm>(data.weights)->get_buffer());
                    auto ptr = std::dynamic_pointer_cast<const ze::gpu_usm>(data.weights)->get_buffer().get();
                    status = zeKernelSetArgumentValue(kernel, i, sizeof(void*), &ptr);
                    ZE_CHECK(status);
                }
                break;
            case args_t::BIAS:
                if (data.bias) {
                    //status = kernel.setArgUsm(i, std::dynamic_pointer_cast<const ze::gpu_usm>(data.bias)->get_buffer());
                    auto ptr = std::dynamic_pointer_cast<const ze::gpu_usm>(data.bias)->get_buffer().get();
                    status = zeKernelSetArgumentValue(kernel, i, sizeof(void*), &ptr);
                    ZE_CHECK(status);
                }
                break;
            case args_t::WEIGHTS_ZERO_POINTS:
                if (data.weights_zero_points) {
                    auto ptr = std::dynamic_pointer_cast<const ze::gpu_usm>(data.weights_zero_points)->get_buffer().get();
                    status = zeKernelSetArgumentValue(kernel, i, sizeof(void*), &ptr);
                    ZE_CHECK(status);
                }
                break;
            default:
                break;
        }
    }
}
}  // namespace
ze_stream::ze_stream(const ze_engine& engine) : stream(engine.configuration().queue_type), _engine(engine) {
    auto context = engine.get_context();
    auto device = engine.get_device();
    auto config = engine.configuration();

    sync_method = _engine.configuration().enable_profiling ? sync_methods::events :
                  config.queue_type == queue_types::out_of_order ? sync_methods::barriers : sync_methods::none;

    if (sync_method == sync_methods::none && config.queue_type == queue_types::out_of_order) {
        throw std::runtime_error("[CLDNN] Unexpected sync method (none) is specified for out_of_order queue");
    }
    //*
    ze_command_queue_desc_t command_queue_desc = {};
    command_queue_desc.flags = 0;
    command_queue_desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    command_queue_desc.pNext = nullptr;
    command_queue_desc.mode = config.queue_type == queue_types::out_of_order ? ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS : ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    command_queue_desc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    ZE_CHECK(zeCommandListCreateImmediate(context, device, &command_queue_desc, &_command_list));
    /*/

    ze_command_list_desc_t command_list_desc = {};
    command_list_desc.flags = 0;
    command_list_desc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    command_list_desc.pNext = nullptr;
    ZE_CHECK(zeCommandListCreate(context, device, &command_list_desc, &_command_list));
    //zeCommandQueueSynchronize
    //*/
    // ze_event_pool_desc_t event_pool_desc = {
    //     ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
    //     nullptr,
    //     ZE_EVENT_POOL_FLAG_HOST_VISIBLE, // all events in pool are visible to Host
    //     100 // count
    // };

    // ZE_CHECK(zeEventPoolCreate(context, &event_pool_desc, 0, nullptr, &_event_pool));
}

void ze_stream::set_arguments(kernel& kernel, const kernel_arguments_desc& args_desc, const kernel_arguments_data& args) {
    static std::mutex m;
    std::lock_guard<std::mutex> guard(m);

    auto& ze_kernel = downcast<ze::ze_kernel>(kernel);
    //ZE_CHECK(zeCommandListAppendBarrier(_command_list, nullptr, 0, nullptr));
    auto& kern = ze_kernel.get_handle();
    set_arguments_impl(kern, args_desc.arguments, args);
}

event::ptr ze_stream::enqueue_kernel(kernel& kernel,
                                     const kernel_arguments_desc& args_desc,
                                     const kernel_arguments_data& /* args */,
                                     std::vector<event::ptr> const& deps,
                                     bool is_output) {
    auto& ze_kernel = downcast<ze::ze_kernel>(kernel);

    auto& kern = ze_kernel.get_handle();

    std::vector<ze_event_handle_t> dep_events;
    std::vector<ze_event_handle_t>* dep_events_ptr = nullptr;
    if (sync_method == sync_methods::events) {
        for (auto& dep : deps) {
            if (auto ze_base_ev = std::dynamic_pointer_cast<ze_base_event>(dep)) {
                if (ze_base_ev->get() != nullptr)
                    dep_events.push_back(ze_base_ev->get());
            }
        }
        dep_events_ptr = &dep_events;
    } else if (sync_method == sync_methods::barriers) {
        sync_events(deps, is_output);
    }
    bool set_output_event = sync_method == sync_methods::events || is_output;

    // try {
    //     _command_queue.enqueueNDRangeKernel(kern, cl::NullRange, global, local, dep_events_ptr, set_output_event ? &ret_ev : nullptr);
    // } catch (cl::Error const& err) {
    //     throw ze_error(err);
    // }
    //ze_event_handle_t ret_ev;
    // ze_event_desc_t eventDesc = {
    //     ZE_STRUCTURE_TYPE_EVENT_DESC,
    //     nullptr,
    //     event_idx++ % 100,       // index
    //     0,                       // no additional memory/cache coherency required on signal
    //     ZE_EVENT_SCOPE_FLAG_DEVICE // ensure memory coherency across device and Host after event completes
    // };
    // zeEventCreate(_event_pool, &eventDesc, &ret_ev);
    auto ev = create_base_event();
    auto global = to_group_count(args_desc.workGroups.global);
    auto local = to_group_count(args_desc.workGroups.local);
    std::cout << local.groupCountX << "," << local.groupCountY << "," << local.groupCountZ << std::endl;
    std::cout << global.groupCountX << "," << global.groupCountY << "," << global.groupCountZ << std::endl;
    //ze_group_count_t launchArgs = { global.groupCountX/local.groupCountX, global.groupCountY/local.groupCountY, global.groupCountZ/local.groupCountZ };
    ze_group_count_t launchArgs = { global.groupCountX, global.groupCountY, global.groupCountZ };
    //ZE_CHECK(zeKernelSetGroupSize(kern, global.groupCountX/local.groupCountX, global.groupCountY/local.groupCountY, global.groupCountZ/local.groupCountZ));
    ZE_CHECK(zeKernelSetGroupSize(kern, local.groupCountX, local.groupCountY, local.groupCountZ));
    ZE_CHECK(zeCommandListAppendLaunchKernel(_command_list,
                                    kern,
                                    &launchArgs,
                                    set_output_event ? std::dynamic_pointer_cast<ze_base_event>(ev)->get() : nullptr,
                                    dep_events_ptr == nullptr ? 0 : dep_events_ptr->size(),
                                    dep_events_ptr == nullptr ? 0 : &dep_events_ptr->at(0)));

    // zeCommandListAppendBarrier(get_queue(), ret_ev2, 0, nullptr);
    // zeEventHostSynchronize(ret_ev2, 0);
    return ev;//std::make_shared<ze_event>(_event_pool, ret_ev, ++_queue_counter);
    //return nullptr;
}

void ze_stream::enqueue_barrier() {
    ZE_CHECK(zeCommandListAppendBarrier(_command_list, nullptr, 0, nullptr));
}

ze_event::ptr ze_stream::enqueue_marker(std::vector<ze_event::ptr> const& deps, bool is_output) {
    if (deps.empty())
        return create_user_event(true);
    ze_event_handle_t ret_ev;
    ze_event_pool_handle_t _event_pool;
    ze_event_pool_desc_t event_pool_desc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE, // all events in pool are visible to Host
        1 // count
    };
    ZE_CHECK(zeEventPoolCreate(_engine.get_context(), &event_pool_desc, 0, nullptr, &_event_pool));
    ze_event_desc_t eventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,//event_idx++ % 100,                 // index
        0,                         // no additional memory/cache coherency required on signal
        ZE_EVENT_SCOPE_FLAG_DEVICE // ensure memory coherency across device and Host after event completes
    };
    ZE_CHECK(zeEventCreate(_event_pool, &eventDesc, &ret_ev));

    if (sync_method  == sync_methods::events) {
        std::vector<ze_event_handle_t> dep_events;
        for (auto& dep : deps) {
            if (auto ze_base_ev = std::dynamic_pointer_cast<ze_base_event>(dep)) {
                if (ze_base_ev->get() != nullptr)
                    dep_events.push_back(ze_base_ev->get());
            }
        }
        if (dep_events.empty())
            return create_user_event(true);
        //_command_queue.enqueueMarkerWithWaitList(&dep_events, &ret_ev);
        ZE_CHECK(zeCommandListAppendBarrier(_command_list, ret_ev, 0, nullptr));
        return std::make_shared<ze_event>(_event_pool, ret_ev, ++_queue_counter);
    } else if (sync_method == sync_methods::barriers) {
        sync_events(deps, is_output);
        return std::make_shared<ze_event>(_event_pool, _last_barrier_ev, _last_barrier);
    } else {
        return std::make_shared<ze_event>(_event_pool, ret_ev, true);
    }
    //immidiatly_comand_queue!!!
}

ze_event::ptr ze_stream::group_events(std::vector<ze_events::ptr> const& deps) {
    return std::make_shared<ze_events>(deps);
}

ze_event::ptr ze_stream::create_user_event(bool set) {
    //return nullptr;
    ze_event_pool_handle_t _event_pool;
    ze_event_pool_desc_t event_pool_desc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE, // all events in pool are visible to Host
        1 // count
    };

    ZE_CHECK(zeEventPoolCreate(_engine.get_context(), &event_pool_desc, 0, nullptr, &_event_pool));
    ze_event_handle_t hEvent;
    ze_event_desc_t tsEventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,         // index
        0,                         // no additional memory/cache coherency required on signal
        ZE_EVENT_SCOPE_FLAG_DEVICE // no additional memory/cache coherency required on wait
    };
    ZE_CHECK(zeEventCreate(_event_pool, &tsEventDesc, &hEvent));
    return std::make_shared<ze_event>(_event_pool, hEvent, set);//_engine.get_context(), set);
}

ze_event::ptr ze_stream::create_base_event() {
    //cl::Event ret_ev;
    ze_event_pool_handle_t _event_pool;
    ze_event_pool_desc_t event_pool_desc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
        nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE, // all events in pool are visible to Host
        1 // count
    };
    ZE_CHECK(zeEventPoolCreate(_engine.get_context(), &event_pool_desc, 0, nullptr, &_event_pool));
    ze_event_handle_t hEvent;
    ze_event_desc_t tsEventDesc = {
        ZE_STRUCTURE_TYPE_EVENT_DESC,
        nullptr,
        0,         // index
        0,                         // no additional memory/cache coherency required on signal
        ZE_EVENT_SCOPE_FLAG_DEVICE // no additional memory/cache coherency required on wait
    };

    ZE_CHECK(zeEventCreate(_event_pool, &tsEventDesc, &hEvent));
    return std::make_shared<ze_event>(_event_pool, hEvent, ++_queue_counter);
}

void ze_stream::flush() const {
    //zeCommandQueueSynchronize
    ZE_CHECK(zeCommandListAppendBarrier(get_queue(), nullptr, 0, nullptr));
    ZE_CHECK(zeCommandListClose(_command_list));
    ze_command_queue_desc_t commandQueueDesc = {
        ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC ,
        nullptr,
        0,
        0,
        0,
        ZE_COMMAND_QUEUE_MODE_DEFAULT ,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL
    };
    ze_command_queue_handle_t hCommandQueue;
    ZE_CHECK(zeCommandQueueCreate(_engine.get_context(), _engine.get_device(), &commandQueueDesc, &hCommandQueue));

    std::vector<ze_command_list_handle_t> vec_command_list;
    vec_command_list.push_back(_command_list);

    ze_fence_desc_t fenceDesc = {
        ZE_STRUCTURE_TYPE_FENCE_DESC ,
        nullptr,
        0
    };
    ze_fence_handle_t hFence;
    ZE_CHECK(zeFenceCreate(hCommandQueue, &fenceDesc, & hFence));
    //ZE_CHECK(zeCommandListAppendBarrier(get_queue(), nullptr, 0, nullptr));
    ZE_CHECK(zeCommandQueueExecuteCommandLists(hCommandQueue, 1, &vec_command_list.at(0), hFence));//hFence
    ZE_CHECK(zeFenceHostSynchronize(hFence, UINT32_MAX));
    ZE_CHECK(zeCommandQueueSynchronize(hCommandQueue, UINT32_MAX));

    // ZE_CHECK(zeCommandQueueSynchronize(hCommandQueue, UINT32_MAX));
    // ze_event_handle_t ret_ev;
    // ze_event_desc_t eventDesc = {
    //     ZE_STRUCTURE_TYPE_EVENT_DESC,
    //     nullptr,
    //     (event_idx+1)%100,         // index
    //     0,                         // no additional memory/cache coherency required on signal
    //     ZE_EVENT_SCOPE_FLAG_DEVICE // ensure memory coherency across device and Host after event completes
    // };
    // ZE_CHECK(zeEventCreate(_event_pool, &eventDesc, &ret_ev));
    // ZE_CHECK(zeEventHostSynchronize(ret_ev, UINT32_MAX));
    // ZE_CHECK(zeCommandListReset(_command_list));
    // zeCommandQueueSynchronize();
    // ZE_CHECK(zeCommandListReset(_command_list));
    // ZE_CHECK(zeFenceReset(hFence));
}

void ze_stream::finish() const {
    //ZE_CHECK(zeCommandListAppendBarrier(get_queue(), nullptr, 0, nullptr));
    flush();
}

void ze_stream::wait_for_events(const std::vector<event::ptr>& events) {
    if (events.empty())
        return;

    std::vector<ze::ze_base_event> _ze_events;
    for (auto& ev : events) {
        if (auto ze_base_ev = dynamic_cast<ze_base_event*>(ev.get())) {
            //_ze_events.push_back(ze_base_ev->get());
            ZE_CHECK(zeEventHostSynchronize(ze_base_ev->get(), UINT32_MAX));
        }
    }
    // std::vector<cl::Event> clevents;
    // for (auto& ev : events) {
    //     if (auto ze_base_ev = dynamic_cast<ze_base_event*>(ev.get()))
    //         clevents.push_back(ze_base_ev->get());
    // }

    // try {
    //     cl::WaitForEvents(clevents);
    // } catch (cl::Error const& err) {
    //     throw ze_error(err);
    // }
}

void ze_stream::sync_events(std::vector<event::ptr> const& deps, bool is_output) {
    bool needs_barrier = false;
    for (auto& dep : deps) {
        auto* ze_base_ev = dynamic_cast<ze_base_event*>(dep.get());
        if (ze_base_ev->get_queue_stamp() > _last_barrier) {
            needs_barrier = true;
        }
    }

    if (needs_barrier) {
        //try {
            if (is_output) {
                //sync_events(deps, is_output);
                // ze_event_pool_handle_t _event_pool;
                // ze_event_pool_desc_t event_pool_desc = {
                //     ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
                //     nullptr,
                //     ZE_EVENT_POOL_FLAG_HOST_VISIBLE, // all events in pool are visible to Host
                //     1 // count
                // };
                // ze_event_desc_t eventDesc = {
                //     ZE_STRUCTURE_TYPE_EVENT_DESC,
                //     nullptr,
                //     0,//event_idx++ % 100,                 // index
                //     0,                         // no additional memory/cache coherency required on signal
                //     ZE_EVENT_SCOPE_FLAG_DEVICE // ensure memory coherency across device and Host after event completes
                // };
                // ZE_CHECK(zeEventPoolCreate(_engine.get_context(), &event_pool_desc, 0, nullptr, &_event_pool));
                // ZE_CHECK(zeEventCreate(_event_pool, &eventDesc, &_last_barrier_ev));
                ZE_CHECK(zeCommandListAppendBarrier(_command_list, nullptr, 0, &_last_barrier_ev));//_last_barrier_ev
            } else {
                ZE_CHECK(zeCommandListAppendBarrier(_command_list, nullptr, 0, nullptr));
            }
        //} catch (ze::Error const& err) {
        //    throw ze_error(err);
        //}
        _last_barrier = ++_queue_counter;
    }
}

}  // namespace ze
}  // namespace cldnn