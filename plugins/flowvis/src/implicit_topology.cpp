#include "stdafx.h"
#include "implicit_topology.h"

#include "glyph_data_call.h"
#include "implicit_topology_call.h"
#include "implicit_topology_computation.h"
#include "implicit_topology_results.h"
#include "mesh_data_call.h"
#include "triangle_mesh_call.h"
#include "triangulation.h"
#include "vector_field_call.h"

#include "mmcore/Call.h"
#include "mmcore/DirectDataWriterCall.h"
#include "mmcore/param/BoolParam.h"
#include "mmcore/param/ButtonParam.h"
#include "mmcore/param/EnumParam.h"
#include "mmcore/param/FloatParam.h"
#include "mmcore/param/FilePathParam.h"
#include "mmcore/param/IntParam.h"
#include "mmcore/param/TransferFunctionParam.h"
#include "mmcore/view/special/CallbackScreenShooter.h"

#include "vislib/math/Rectangle.h"
#include "vislib/sys/Log.h"

#include "Eigen/Dense"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace megamol
{
    namespace flowvis
    {
        implicit_topology::implicit_topology() :
            triangle_mesh_slot("set_triangle_mesh", "Triangle mesh output"),
            mesh_data_slot("set_mesh_data", "Mesh data output"),
            result_writer_slot("result_writer_slot", "Results output slot"),
            screenshot_slot("screenshot_slot", "Screenshot output slot"),
            log_slot("log_slot", "Log output slot"),
            performance_slot("performance_slot", "Performance log output slot"),
            vector_field_slot("vector_field_slot", "Vector field input slot"),
            convergence_structures_slot("convergence_structures_slot", "Convergence structures input slot"),
            result_reader_slot("result_reader_slot", "Results input slot"),
            start_computation("start_computation", "Start the computation"),
            stop_computation("stop_computation", "Stop the computation"),
            reset_computation("reset_computation", "Reset the computation"),
            load_computation("load_computation", "Load computation from file"),
            save_computation("save_computation", "Save computation to file"),
            label_transfer_function("label_transfer_function", "Transfer function for labels"),
            label_fixed_range("label_fixed_range", "Fixed or dynamic value range for labels"),
            label_range_min("label_range_min", "Minimum value for labels in the transfer function"),
            label_range_max("label_range_max", "Maximum value for labels in the transfer function"),
            num_labels_combined("num_labels_combined", "Number of labels in the combined label field"),
            distance_transfer_function("distance_transfer_function", "Transfer function for distances"),
            distance_fixed_range("distance_fixed_range", "Fixed or dynamic value range for labels"),
            distance_range_min("distance_range_min", "Minimum value for distances in the transfer function"),
            distance_range_max("distance_range_max", "Maximum value for distances in the transfer function"),
            termination_transfer_function("termination_transfer_function", "Transfer function for reasons of termination"),
            termination_fixed_range("termination_fixed_range", "Fixed or dynamic value range for reasons of termination"),
            termination_range_min("termination_range_min", "Minimum value for reasons of termination in the transfer function"),
            termination_range_max("termination_range_max", "Maximum value for reasons of termination in the transfer function"),
            gradient_transfer_function("gradient_transfer_function", "Transfer function for gradients"),
            gradient_fixed_range("gradient_fixed_range", "Fixed or dynamic value range for gradients"),
            gradient_range_min("gradient_range_min", "Minimum value for gradients in the transfer function"),
            gradient_range_max("gradient_range_max", "Maximum value for gradients in the transfer function"),
            integration_method("integration_method", "Method for stream line integration"),
            num_integration_steps("num_integration_steps", "Number of stream line integration steps"),
            integration_timestep("integration_timestep", "Initial time step for stream line integration"),
            max_integration_error("max_integration_error", "Maximum integration error for Runge-Kutta 4-5"),
            num_particles_per_batch("num_particles_per_batch", "Number of particles per batch (influences GPU utilization)"),
            num_integration_steps_per_batch("num_integration_steps_per_batch", "Number of integration steps per batch, after which a result can be visualized"),
            refinement_threshold("refinement_threshold", "Threshold for grid refinement, defined as minimum edge length"),
            refine_at_labels("refine_at_labels", "Should the grid be refined in regions of different labels?"),
            distance_difference_threshold("distance_difference_threshold", "Threshold for refining the grid when neighboring nodes exceed a distance difference"),
            auto_save_results("auto_save_results", "Automatically save results when new ones are available"),
            auto_save_screenshots("auto_save_screenshots", "Automatically take screenshot when new results are available"),
            computation_running(false), mesh_output_changed(false), data_output_changed(false), computation(nullptr), previous_result(nullptr)
        {
            // Connect output
            this->triangle_mesh_slot.SetCallback(triangle_mesh_call::ClassName(), triangle_mesh_call::FunctionName(0), &implicit_topology::get_triangle_data_callback);
            this->triangle_mesh_slot.SetCallback(triangle_mesh_call::ClassName(), triangle_mesh_call::FunctionName(1), &implicit_topology::get_triangle_extent_callback);
            this->MakeSlotAvailable(&this->triangle_mesh_slot);

            this->mesh_data_slot.SetCallback(mesh_data_call::ClassName(), mesh_data_call::FunctionName(0), &implicit_topology::get_data_data_callback);
            this->mesh_data_slot.SetCallback(mesh_data_call::ClassName(), mesh_data_call::FunctionName(1), &implicit_topology::get_data_extent_callback);
            this->MakeSlotAvailable(&this->mesh_data_slot);

            this->result_writer_slot.SetCallback(implicit_topology_writer_call::ClassName(), implicit_topology_writer_call::FunctionName(0), &implicit_topology::get_result_writer_cb_callback);
            this->MakeSlotAvailable(&this->result_writer_slot);
            this->get_result_writer_callback = [](const implicit_topology_results&) -> bool {
                vislib::sys::Log::DefaultLog.WriteWarn("Cannot write results. Writer module not connected!"); return true; };

            this->screenshot_slot.SetCallback(core::view::special::CallbackScreenShooterCall::ClassName(),
                core::view::special::CallbackScreenShooterCall::FunctionName(0), &implicit_topology::get_screenshot_cb_callback);
            this->MakeSlotAvailable(&this->screenshot_slot);
            this->get_screenshot_callback = []() -> void { vislib::sys::Log::DefaultLog.WriteWarn("Cannot take screenshot. Screen shooter module not connected!"); };
            
            this->log_slot.SetCallback(core::DirectDataWriterCall::ClassName(), core::DirectDataWriterCall::FunctionName(0), &implicit_topology::get_log_cb_callback);
            this->MakeSlotAvailable(&this->log_slot);
            this->get_log_callback = []() -> std::ostream& { static std::ostream dummy(nullptr); return dummy; };

            this->performance_slot.SetCallback(core::DirectDataWriterCall::ClassName(), core::DirectDataWriterCall::FunctionName(0), &implicit_topology::get_performance_cb_callback);
            this->MakeSlotAvailable(&this->performance_slot);
            this->get_performance_callback = []() -> std::ostream& { static std::ostream dummy(nullptr); return dummy; };

            // Connect input
            this->vector_field_slot.SetCompatibleCall<vector_field_call::vector_field_description>();
            this->MakeSlotAvailable(&this->vector_field_slot);

            this->convergence_structures_slot.SetCompatibleCall<glyph_data_call::glyph_data_description>();
            this->MakeSlotAvailable(&this->convergence_structures_slot);

            this->result_reader_slot.SetCompatibleCall<implicit_topology_reader_call::implicit_topology_reader_description>();
            this->MakeSlotAvailable(&this->result_reader_slot);

            // Create computation parameters
            this->integration_method << new core::param::EnumParam(0);
            this->integration_method.Param<core::param::EnumParam>()->SetTypePair(0, "Runge-Kutta 4 (fixed)");
            this->integration_method.Param<core::param::EnumParam>()->SetTypePair(1, "Runge-Kutta 4-5 (dynamic)");
            this->MakeSlotAvailable(&this->integration_method);

            this->num_integration_steps << new core::param::IntParam(0);
            this->MakeSlotAvailable(&this->num_integration_steps);

            this->integration_timestep << new core::param::FloatParam(0.01f);
            this->MakeSlotAvailable(&this->integration_timestep);

            this->max_integration_error << new core::param::FloatParam(0.000001f);
            this->MakeSlotAvailable(&this->max_integration_error);

            this->num_particles_per_batch << new core::param::IntParam(10000);
            this->MakeSlotAvailable(&this->num_particles_per_batch);

            this->num_integration_steps_per_batch << new core::param::IntParam(10000);
            this->MakeSlotAvailable(&this->num_integration_steps_per_batch);

            this->refinement_threshold << new core::param::FloatParam(0.00024f);
            this->MakeSlotAvailable(&this->refinement_threshold);

            this->refine_at_labels << new core::param::BoolParam(true);
            this->MakeSlotAvailable(&this->refine_at_labels);

            this->distance_difference_threshold << new core::param::FloatParam(0.00025f);
            this->MakeSlotAvailable(&this->distance_difference_threshold);

            // Create computation buttons
            this->start_computation << new core::param::ButtonParam();
            this->start_computation.SetUpdateCallback(&implicit_topology::start_computation_callback);
            this->MakeSlotAvailable(&this->start_computation);

            this->stop_computation << new core::param::ButtonParam();
            this->stop_computation.SetUpdateCallback(&implicit_topology::stop_computation_callback);
            this->MakeSlotAvailable(&this->stop_computation);

            this->reset_computation << new core::param::ButtonParam();
            this->reset_computation.SetUpdateCallback(&implicit_topology::reset_computation_callback);
            this->MakeSlotAvailable(&this->reset_computation);

            this->load_computation << new core::param::ButtonParam();
            this->load_computation.SetUpdateCallback(&implicit_topology::load_computation_callback);
            this->MakeSlotAvailable(&this->load_computation);

            this->save_computation << new core::param::ButtonParam();
            this->save_computation.SetUpdateCallback(&implicit_topology::save_computation_callback);
            this->MakeSlotAvailable(&this->save_computation);

            // Create auto-save and auto-screenshot checkboxes
            this->auto_save_results << new core::param::BoolParam(false);
            this->MakeSlotAvailable(&this->auto_save_results);

            this->auto_save_screenshots << new core::param::BoolParam(false);
            this->MakeSlotAvailable(&this->auto_save_screenshots);

            // Create transfer function parameters
            this->label_transfer_function << new core::param::TransferFunctionParam(
                "{\"Interpolation\":\"LINEAR\",\"Nodes\":[[0.0,0.0,0.423499,1.0,0.0,0.05],[0.0,0.119346,0.529237,1.0,0.125,0.05]," \
                "[0.0,0.238691,0.634976,1.0,0.1875,0.05],[0.0,0.346852,0.68788,1.0,0.25,0.05],[0.0,0.45022,0.718141,1.0,0.3125,0.05]," \
                "[0.0,0.553554,0.664839,1.0,0.375,0.05],[0.0,0.651082,0.519303,1.0,0.4375,0.05],[0.115841,0.72479,0.352857,1.0,0.5,0.05]," \
                "[0.326771,0.781195,0.140187,1.0,0.5625,0.05],[0.522765,0.798524,0.0284624,1.0,0.625,0.05],[0.703162,0.788685,0.00885756,1.0,0.6875,0.05]," \
                "[0.845118,0.751133,0.0,1.0,0.75,0.05],[0.955734,0.690825,0.0,1.0,0.8125,0.05],[0.995402,0.567916,0.0618524,1.0,0.875,0.05]," \
                "[0.987712,0.403398,0.164851,1.0,0.9375,0.05],[0.980407,0.247105,0.262699,1.0,1.0,0.05]],\"ValueRange\":[0.0,1.0],\"TextureSize\":128}");
            this->MakeSlotAvailable(&this->label_transfer_function);

            this->label_fixed_range << new core::param::BoolParam(false);
            this->MakeSlotAvailable(&this->label_fixed_range);

            this->num_labels_combined << new core::param::IntParam(0);
            this->num_labels_combined.Parameter()->SetGUIReadOnly(true);
            this->MakeSlotAvailable(&this->num_labels_combined);

            this->label_range_min << new core::param::FloatParam(0.0f);
            this->label_range_min.Parameter()->SetGUIReadOnly(true);
            this->MakeSlotAvailable(&this->label_range_min);

            this->label_range_max << new core::param::FloatParam(1.0f);
            this->label_range_max.Parameter()->SetGUIReadOnly(true);
            this->MakeSlotAvailable(&this->label_range_max);

            this->distance_transfer_function << new core::param::TransferFunctionParam(
                "{\"Interpolation\":\"LINEAR\",\"Nodes\":[[0.0,0.0,0.0,1.0,0.0,0.05],[0.9019607901573181,0.0,0.0,1.0,0.39500004053115845,0.05]," \
                "[0.9019607901573181,0.9019607901573181,0.0,1.0,0.7990000247955322,0.05],[1.0,1.0,1.0,1.0,1.0,0.05]],\"ValueRange\":[0.0,1.0],\"TextureSize\":128}");
            this->MakeSlotAvailable(&this->distance_transfer_function);

            this->distance_fixed_range << new core::param::BoolParam(false);
            this->MakeSlotAvailable(&this->distance_fixed_range);

            this->distance_range_min << new core::param::FloatParam(0.0f);
            this->distance_range_min.Parameter()->SetGUIReadOnly(true);
            this->MakeSlotAvailable(&this->distance_range_min);

            this->distance_range_max << new core::param::FloatParam(1.0f);
            this->distance_range_max.Parameter()->SetGUIReadOnly(true);
            this->MakeSlotAvailable(&this->distance_range_max);

            this->termination_transfer_function << new core::param::TransferFunctionParam(
                "{\"Interpolation\":\"LINEAR\",\"Nodes\":[[0.23137255012989044,0.2980392277240753,0.7529411911964417,1.0,0.0,0.05]," \
                "[0.8627451062202454,0.8627451062202454,0.8627451062202454,1.0,0.4989999830722809,0.05]," \
                "[0.7058823704719543,0.01568627543747425,0.14901961386203766,1.0,1.0,0.05]],\"ValueRange\":[0.0,1.0],\"TextureSize\":4}");
            this->MakeSlotAvailable(&this->termination_transfer_function);

            this->termination_fixed_range << new core::param::BoolParam(true);
            this->MakeSlotAvailable(&this->termination_fixed_range);

            this->termination_range_min << new core::param::FloatParam(-1.0f);
            this->MakeSlotAvailable(&this->termination_range_min);

            this->termination_range_max << new core::param::FloatParam(2.0f);
            this->MakeSlotAvailable(&this->termination_range_max);

            this->gradient_transfer_function << new core::param::TransferFunctionParam(
                "{\"Interpolation\":\"LINEAR\",\"Nodes\":[[1.0,1.0,1.0,1.0,0.0,0.05],[0.0,0.0,0.0,1.0,1.0,0.05]],\"ValueRange\":[0.0,1.0],\"TextureSize\":128}");
            this->MakeSlotAvailable(&this->gradient_transfer_function);

            this->gradient_fixed_range << new core::param::BoolParam(false);
            this->MakeSlotAvailable(&this->gradient_fixed_range);

            this->gradient_range_min << new core::param::FloatParam(0.0f);
            this->gradient_range_min.Parameter()->SetGUIReadOnly(true);
            this->MakeSlotAvailable(&this->gradient_range_min);

            this->gradient_range_max << new core::param::FloatParam(1.0f);
            this->gradient_range_max.Parameter()->SetGUIReadOnly(true);
            this->MakeSlotAvailable(&this->gradient_range_max);
        }

        implicit_topology::~implicit_topology()
        {
            this->Release();

            if (this->computation != nullptr)
            {
                this->computation->terminate();
            }
        }

        bool implicit_topology::create()
        {
            return true;
        }

        void implicit_topology::release()
        {
        }

        bool implicit_topology::initialize_computation()
        {
            // Try to load input vector field
            if (this->computation == nullptr)
            {
                std::array<unsigned int, 2> resolution;
                std::array<float, 4> domain;
                
                std::vector<float> positions;
                std::vector<float> vectors;
                std::vector<float> points;
                std::vector<int> point_ids;
                std::vector<float> lines;
                std::vector<int> line_ids;

                if (load_input(resolution, domain, positions, vectors, points, point_ids, lines, line_ids))
                {
                    // Create new computation object
                    this->computation = std::make_unique<implicit_topology_computation>(this->get_log_callback(), this->get_performance_callback(),
                        std::move(resolution), std::move(domain), std::move(positions), std::move(vectors), std::move(points), std::move(point_ids),
                        std::move(lines), std::move(line_ids),
                        this->integration_timestep.Param<core::param::FloatParam>()->Value(),
                        this->max_integration_error.Param<core::param::FloatParam>()->Value(),
                        static_cast<streamlines_cuda::integration_method>(this->integration_method.Param<core::param::EnumParam>()->Value()));

                    set_readonly_fixed_parameters(true);

                    return true;
                }

                return false;
            }

            return true;
        }

        bool implicit_topology::load_input(std::array<unsigned int, 2>& resolution, std::array<float, 4>& domain, std::vector<float>& positions,
            std::vector<float>& vectors, std::vector<float>& points, std::vector<int>& point_ids, std::vector<float>& lines, std::vector<int>& line_ids)
        {
            // Get vector field
            auto* vf_call = this->vector_field_slot.CallAs<vector_field_call>();

            if (vf_call != nullptr && (*vf_call)(1) && (*vf_call)(0))
            {
                resolution = this->resolution = vf_call->get_resolution();
                domain = { vf_call->get_bounding_rectangle().Left(), vf_call->get_bounding_rectangle().Bottom(),
                    vf_call->get_bounding_rectangle().Right(), vf_call->get_bounding_rectangle().Top() };

                positions = *vf_call->get_positions();
                vectors = *vf_call->get_vectors();
            }
            else
            {
                return false;
            }

            // Load convergence structures
            auto* glyph_call = this->convergence_structures_slot.CallAs<glyph_data_call>();

            if (glyph_call != nullptr && (*glyph_call)(1) && (*glyph_call)(0))
            {
                // Get points
                const auto& input_points = glyph_call->get_points();

                points.reserve(2 * input_points.size());
                point_ids.reserve(input_points.size());

                for (const auto& point : input_points)
                {
                    points.push_back(point.first[0]);
                    points.push_back(point.first[1]);

                    point_ids.push_back(static_cast<int>(point.second));
                }

                // Get lines
                const auto& input_lines = glyph_call->get_line_segments();

                lines.reserve(4 * input_lines.size());
                line_ids.reserve(input_lines.size());

                for (const auto& line : input_lines)
                {
                    lines.push_back(line.first.first[0]);
                    lines.push_back(line.first.first[1]);
                    lines.push_back(line.first.second[0]);
                    lines.push_back(line.first.second[1]);

                    line_ids.push_back(static_cast<int>(line.second));
                }
            }
            else
            {
                return false;
            }

            return true;
        }

        void implicit_topology::update_results()
        {
            // Try to get new results
            if (this->computation_running && !(this->mesh_output_changed || this->data_output_changed))
            {
                // Get new results
                if (this->last_result.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready)
                {
                    return;
                }

                vislib::sys::Log::DefaultLog.WriteInfo("Computation of topology yielded new results.");

                // Store triangles
                auto result = this->last_result.get();

                this->vertices = result.vertices;
                this->indices = result.indices;

                this->labels_forward = result.labels_forward;
                this->distances_forward = result.distances_forward;
                this->terminations_forward = result.terminations_forward;

                this->labels_backward = result.labels_backward;
                this->distances_backward = result.distances_backward;
                this->terminations_backward = result.terminations_backward;

                this->computation_running = !result.computation_state.finished;

                if (result.computation_state.finished)
                {
                    vislib::sys::Log::DefaultLog.WriteInfo("Computation of topology ended.");

                    // Reset parameters to read-write
                    set_readonly_variable_parameters(false);
                }

                // Save new last result
                this->last_result = this->computation->get_results();
                this->previous_result = std::make_unique<implicit_topology_results>(result);

                // Save result to file, and take screenshot
                if (this->auto_save_results.Param<core::param::BoolParam>()->Value())
                {
                    this->get_result_writer_callback(result);
                }

                if (this->auto_save_screenshots.Param<core::param::BoolParam>()->Value())
                {
                    this->get_screenshot_callback();
                }

                this->mesh_output_changed = true;
                this->data_output_changed = true;
            }
        }

        void implicit_topology::set_readonly_fixed_parameters(const bool read_only)
        {
            this->integration_method.Parameter()->SetGUIReadOnly(read_only);
            this->integration_timestep.Parameter()->SetGUIReadOnly(read_only);
            this->max_integration_error.Parameter()->SetGUIReadOnly(read_only);
        }

        void implicit_topology::set_readonly_variable_parameters(const bool read_only)
        {
            this->num_integration_steps.Parameter()->SetGUIReadOnly(read_only);
            this->num_particles_per_batch.Parameter()->SetGUIReadOnly(read_only);
            this->num_integration_steps_per_batch.Parameter()->SetGUIReadOnly(read_only);

            this->refinement_threshold.Parameter()->SetGUIReadOnly(read_only);
            this->refine_at_labels.Parameter()->SetGUIReadOnly(read_only);
            this->distance_difference_threshold.Parameter()->SetGUIReadOnly(read_only);
        }

        bool implicit_topology::get_triangle_data_callback(core::Call& call)
        {
            auto* triangle_call = dynamic_cast<triangle_mesh_call*>(&call);
            if (triangle_call == nullptr) return false;

            // Update render output if there are new results
            update_results();

            if (this->mesh_output_changed)
            {
                triangle_call->set_vertices(this->vertices);
                triangle_call->set_indices(this->indices);

                triangle_call->SetDataHash(triangle_call->DataHash() + 1);

                this->mesh_output_changed = false;
            }
            
            return true;
        }

        bool implicit_topology::get_triangle_extent_callback(core::Call& call)
        {
            auto* triangle_call = dynamic_cast<triangle_mesh_call*>(&call);
            if (triangle_call == nullptr) return false;

            // Get input vector field extents
            auto* vf_call = this->vector_field_slot.CallAs<vector_field_call>();

            if (vf_call != nullptr && (*vf_call)(1))
            {
                this->resolution = vf_call->get_resolution();

                triangle_call->set_dimension(triangle_mesh_call::dimension_t::TWO);
                triangle_call->set_bounding_rectangle(vf_call->get_bounding_rectangle());
            }
            else
            {
                return false;
            }

            return true;
        }

        bool implicit_topology::get_data_data_callback(core::Call& call)
        {
            auto* data_call = dynamic_cast<mesh_data_call*>(&call);
            if (data_call == nullptr) return false;

            // Set accessibility
            this->label_range_min.Parameter()->SetGUIReadOnly(!this->label_fixed_range.Param<core::param::BoolParam>()->Value());
            this->label_range_max.Parameter()->SetGUIReadOnly(!this->label_fixed_range.Param<core::param::BoolParam>()->Value());

            this->distance_range_min.Parameter()->SetGUIReadOnly(!this->distance_fixed_range.Param<core::param::BoolParam>()->Value());
            this->distance_range_max.Parameter()->SetGUIReadOnly(!this->distance_fixed_range.Param<core::param::BoolParam>()->Value());

            this->termination_range_min.Parameter()->SetGUIReadOnly(!this->termination_fixed_range.Param<core::param::BoolParam>()->Value());
            this->termination_range_max.Parameter()->SetGUIReadOnly(!this->termination_fixed_range.Param<core::param::BoolParam>()->Value());

            this->gradient_range_min.Parameter()->SetGUIReadOnly(!this->gradient_fixed_range.Param<core::param::BoolParam>()->Value());
            this->gradient_range_max.Parameter()->SetGUIReadOnly(!this->gradient_fixed_range.Param<core::param::BoolParam>()->Value());

            // Only update if there is actual data
            if (this->labels_forward == nullptr || this->labels_backward == nullptr || this->distances_forward == nullptr ||
                this->distances_backward == nullptr || this->terminations_forward == nullptr || this->terminations_backward == nullptr)
            {
                return true;
            }

            // Update render output if there are new results
            update_results();

            if (this->data_output_changed
                || this->label_fixed_range.IsDirty() || this->label_range_min.IsDirty() || this->label_range_max.IsDirty()
                || this->distance_fixed_range.IsDirty() || this->distance_range_min.IsDirty() || this->distance_range_max.IsDirty()
                || this->termination_fixed_range.IsDirty() || this->termination_range_min.IsDirty() || this->termination_range_max.IsDirty()
                || this->gradient_fixed_range.IsDirty() || this->gradient_range_min.IsDirty() || this->gradient_range_max.IsDirty())
            {
                this->label_fixed_range.ResetDirty();
                this->label_range_min.ResetDirty();
                this->label_range_max.ResetDirty();

                this->distance_fixed_range.ResetDirty();
                this->distance_range_min.ResetDirty();
                this->distance_range_max.ResetDirty();

                this->termination_fixed_range.ResetDirty();
                this->termination_range_min.ResetDirty();
                this->termination_range_max.ResetDirty();

                this->gradient_fixed_range.ResetDirty();
                this->gradient_range_min.ResetDirty();
                this->gradient_range_max.ResetDirty();

                // Set data function
                auto set_data = [](mesh_data_call* call, std::shared_ptr<std::vector<float>> data, const std::string& name,
                    const bool fixed_range, const float range_min, const float range_max) -> std::pair<float, float>
                {
                    auto data_set = std::make_shared<mesh_data_call::data_set>();

                    if (fixed_range)
                    {
                        data_set->min_value = range_min;
                        data_set->max_value = range_max;
                    }
                    else
                    {
                        const auto min_max_value = std::minmax_element(data->begin(), data->end());
                        data_set->min_value = *min_max_value.first;
                        data_set->max_value = *min_max_value.second;
                    }

                    data_set->data = data;

                    call->set_data(name, data_set);

                    return { data_set->min_value, data_set->max_value };
                };

                // Set labels
                if (this->data_output_changed)
                {
                    // Generate unique labels from combinations
                    this->labels = std::make_shared<std::vector<float>>(this->labels_forward->size());

                    auto less = [](const std::pair<float, float>& lhs, const std::pair<float, float>& rhs) -> std::size_t
                    { return lhs.first < rhs.first || (lhs.first == rhs.first && lhs.second < rhs.second); };

                    std::map<std::pair<float, float>, float, decltype(less)> label_combinations(less);

                    int counter = 0;

                    for (std::size_t i = 0; i < this->labels->size(); ++i)
                    {
                        const auto min_max = std::minmax((*this->labels_forward)[i], (*this->labels_backward)[i]);

                        if (label_combinations.find(min_max) == label_combinations.end())
                        {
                            label_combinations[min_max] = static_cast<float>(counter++);
                        }

                        (*this->labels)[i] = label_combinations.at(min_max);
                    }
                }

                auto label_min_max = set_data(data_call, this->labels, "labels",
                    this->label_fixed_range.Param<core::param::BoolParam>()->Value(),
                    this->label_range_min.Param<core::param::FloatParam>()->Value(),
                    this->label_range_max.Param<core::param::FloatParam>()->Value());

                auto label_forward_min_max = set_data(data_call, this->labels_forward, "labels (forward)",
                    this->label_fixed_range.Param<core::param::BoolParam>()->Value(),
                    this->label_range_min.Param<core::param::FloatParam>()->Value(),
                    this->label_range_max.Param<core::param::FloatParam>()->Value());

                auto label_backward_min_max = set_data(data_call, this->labels_backward, "labels (backward)",
                    this->label_fixed_range.Param<core::param::BoolParam>()->Value(),
                    this->label_range_min.Param<core::param::FloatParam>()->Value(),
                    this->label_range_max.Param<core::param::FloatParam>()->Value());

                const float label_min = std::min(label_forward_min_max.first, label_backward_min_max.first);
                const float label_max = std::max(label_forward_min_max.second, label_backward_min_max.second);

                this->label_range_min.Param<core::param::FloatParam>()->SetValue(label_min, false);
                this->label_range_max.Param<core::param::FloatParam>()->SetValue(label_max, false);

                this->num_labels_combined.Param<core::param::IntParam>()->SetValue(static_cast<int>(label_min_max.second - label_min_max.first) + 1, false);

                this->label_transfer_function.ForceSetDirty();
                
                // Set distances
                if (this->data_output_changed)
                {
                    // Generate combination of distances
                    this->distances = std::make_shared<std::vector<float>>(this->distances_forward->size());

                    for (std::size_t i = 0; i < this->distances->size(); ++i)
                    {
                        (*this->distances)[i] = std::sqrt((*this->distances_forward)[i] * (*this->distances_forward)[i]
                            + (*this->distances_backward)[i] * (*this->distances_backward)[i]) / std::sqrt(2.0f);
                    }
                }

                set_data(data_call, this->distances, "distances",
                    this->distance_fixed_range.Param<core::param::BoolParam>()->Value(),
                    this->distance_range_min.Param<core::param::FloatParam>()->Value(),
                    this->distance_range_max.Param<core::param::FloatParam>()->Value());

                auto distance_forward_min_max = set_data(data_call, this->distances_forward, "distances (forward)",
                    this->distance_fixed_range.Param<core::param::BoolParam>()->Value(),
                    this->distance_range_min.Param<core::param::FloatParam>()->Value(),
                    this->distance_range_max.Param<core::param::FloatParam>()->Value());

                auto distance_backward_min_max = set_data(data_call, this->distances_backward, "distances (backward)",
                    this->distance_fixed_range.Param<core::param::BoolParam>()->Value(),
                    this->distance_range_min.Param<core::param::FloatParam>()->Value(),
                    this->distance_range_max.Param<core::param::FloatParam>()->Value());

                const float distance_min = std::min(distance_forward_min_max.first, distance_backward_min_max.first);
                const float distance_max = std::max(distance_forward_min_max.second, distance_backward_min_max.second);

                this->distance_range_min.Param<core::param::FloatParam>()->SetValue(distance_min, false);
                this->distance_range_max.Param<core::param::FloatParam>()->SetValue(distance_max, false);

                this->distance_transfer_function.ForceSetDirty();

                // Set reasons for termination
                auto termination_forward_min_max = set_data(data_call, this->terminations_forward, "reasons for termination (forward)",
                    this->termination_fixed_range.Param<core::param::BoolParam>()->Value(),
                    this->termination_range_min.Param<core::param::FloatParam>()->Value(),
                    this->termination_range_max.Param<core::param::FloatParam>()->Value());

                auto termination_backward_min_max = set_data(data_call, this->terminations_backward, "reasons for termination (backward)",
                    this->termination_fixed_range.Param<core::param::BoolParam>()->Value(),
                    this->termination_range_min.Param<core::param::FloatParam>()->Value(),
                    this->termination_range_max.Param<core::param::FloatParam>()->Value());

                const float termination_min = std::min(termination_forward_min_max.first, termination_backward_min_max.first);
                const float termination_max = std::max(termination_forward_min_max.second, termination_backward_min_max.second);

                this->termination_range_min.Param<core::param::FloatParam>()->SetValue(termination_min, false);
                this->termination_range_max.Param<core::param::FloatParam>()->SetValue(termination_max, false);

                this->termination_transfer_function.ForceSetDirty();

                // Compute and set gradient magnitudes
                if (this->data_output_changed)
                {
                    auto& gradients = *(this->gradients = std::make_shared<std::vector<float>>(this->distances_forward->size()));
                    auto& gradients_forward = *(this->gradients_forward = std::make_shared<std::vector<float>>(this->distances_forward->size(), 0.0f));
                    auto& gradients_backward = *(this->gradients_backward = std::make_shared<std::vector<float>>(this->distances_backward->size(), 0.0f));

                    const auto& vertices = *this->vertices;
                    const auto& indices = *this->indices;

                    const auto& distances_forward = *this->distances_forward;
                    const auto& distances_backward = *this->distances_backward;

                    for (std::size_t i = 0; i < indices.size(); i += 3)
                    {
                        const Eigen::Vector2f point_1(vertices[indices[i + 0] * 2 + 0], vertices[indices[i + 0] * 2 + 1]);
                        const Eigen::Vector2f point_2(vertices[indices[i + 1] * 2 + 0], vertices[indices[i + 1] * 2 + 1]);
                        const Eigen::Vector2f point_3(vertices[indices[i + 2] * 2 + 0], vertices[indices[i + 2] * 2 + 1]);

                        const Eigen::Vector2f distance_1(distances_forward[indices[i + 0]], distances_backward[indices[i + 0]]);
                        const Eigen::Vector2f distance_2(distances_forward[indices[i + 1]], distances_backward[indices[i + 1]]);
                        const Eigen::Vector2f distance_3(distances_forward[indices[i + 2]], distances_backward[indices[i + 2]]);

                        gradients_forward[indices[i + 0]] = std::max(gradients_forward[indices[i + 0]], std::abs(distance_1[0] - distance_2[0]) / (point_1 - point_2).norm());
                        gradients_forward[indices[i + 1]] = std::max(gradients_forward[indices[i + 1]], std::abs(distance_1[0] - distance_2[0]) / (point_1 - point_2).norm());
                        gradients_forward[indices[i + 0]] = std::max(gradients_forward[indices[i + 0]], std::abs(distance_1[0] - distance_3[0]) / (point_1 - point_3).norm());
                        gradients_forward[indices[i + 2]] = std::max(gradients_forward[indices[i + 2]], std::abs(distance_1[0] - distance_3[0]) / (point_1 - point_3).norm());
                        gradients_forward[indices[i + 1]] = std::max(gradients_forward[indices[i + 1]], std::abs(distance_2[0] - distance_3[0]) / (point_2 - point_3).norm());
                        gradients_forward[indices[i + 2]] = std::max(gradients_forward[indices[i + 2]], std::abs(distance_2[0] - distance_3[0]) / (point_2 - point_3).norm());

                        gradients_backward[indices[i + 0]] = std::max(gradients_backward[indices[i + 0]], std::abs(distance_1[1] - distance_2[1]) / (point_1 - point_2).norm());
                        gradients_backward[indices[i + 1]] = std::max(gradients_backward[indices[i + 1]], std::abs(distance_1[1] - distance_2[1]) / (point_1 - point_2).norm());
                        gradients_backward[indices[i + 0]] = std::max(gradients_backward[indices[i + 0]], std::abs(distance_1[1] - distance_3[1]) / (point_1 - point_3).norm());
                        gradients_backward[indices[i + 2]] = std::max(gradients_backward[indices[i + 2]], std::abs(distance_1[1] - distance_3[1]) / (point_1 - point_3).norm());
                        gradients_backward[indices[i + 1]] = std::max(gradients_backward[indices[i + 1]], std::abs(distance_2[1] - distance_3[1]) / (point_2 - point_3).norm());
                        gradients_backward[indices[i + 2]] = std::max(gradients_backward[indices[i + 2]], std::abs(distance_2[1] - distance_3[1]) / (point_2 - point_3).norm());

                        gradients[indices[i + 0]] = std::max(gradients_forward[indices[i + 0]], gradients_backward[indices[i + 0]]);
                        gradients[indices[i + 1]] = std::max(gradients_forward[indices[i + 1]], gradients_backward[indices[i + 1]]);
                        gradients[indices[i + 2]] = std::max(gradients_forward[indices[i + 2]], gradients_backward[indices[i + 2]]);
                    }
                }

                set_data(data_call, this->gradients, "gradients",
                    this->gradient_fixed_range.Param<core::param::BoolParam>()->Value(),
                    this->gradient_range_min.Param<core::param::FloatParam>()->Value(),
                    this->gradient_range_max.Param<core::param::FloatParam>()->Value());

                auto gradient_forward_min_max = set_data(data_call, this->gradients_forward, "gradients (forward)",
                    this->gradient_fixed_range.Param<core::param::BoolParam>()->Value(),
                    this->gradient_range_min.Param<core::param::FloatParam>()->Value(),
                    this->gradient_range_max.Param<core::param::FloatParam>()->Value());

                auto gradient_backward_min_max = set_data(data_call, this->gradients_backward, "gradients (backward)",
                    this->gradient_fixed_range.Param<core::param::BoolParam>()->Value(),
                    this->gradient_range_min.Param<core::param::FloatParam>()->Value(),
                    this->gradient_range_max.Param<core::param::FloatParam>()->Value());

                const float gradient_min = std::min(gradient_forward_min_max.first, gradient_backward_min_max.first);
                const float gradient_max = std::max(gradient_forward_min_max.second, gradient_backward_min_max.second);

                this->gradient_range_min.Param<core::param::FloatParam>()->SetValue(gradient_min, false);
                this->gradient_range_max.Param<core::param::FloatParam>()->SetValue(gradient_max, false);

                this->gradient_transfer_function.ForceSetDirty();

                // Compute and set validity mask
                if (this->data_output_changed)
                {
                    auto& valid_all = *(this->valid_all = std::make_shared<std::vector<GLfloat>>(this->labels->size(), 1.0f));
                    auto& valid_one = *(this->valid_one = std::make_shared<std::vector<GLfloat>>(this->labels->size(), 1.0f));
                    auto& valid_forward = *(this->valid_forward = std::make_shared<std::vector<GLfloat>>(this->labels->size(), 1.0f));
                    auto& valid_backward = *(this->valid_backward = std::make_shared<std::vector<GLfloat>>(this->labels->size(), 1.0f));

                    const auto& indices = *this->indices;

                    const auto& terminations_forward = *this->terminations_forward;
                    const auto& terminations_backward = *this->terminations_backward;

                    for (std::size_t i = 0; i < indices.size(); i += 3)
                    {
                        const Eigen::Vector2f termination_1(terminations_forward[indices[i + 0]], terminations_backward[indices[i + 0]]);
                        const Eigen::Vector2f termination_2(terminations_forward[indices[i + 1]], terminations_backward[indices[i + 1]]);
                        const Eigen::Vector2f termination_3(terminations_forward[indices[i + 2]], terminations_backward[indices[i + 2]]);

                        // Set boundaries to invalid
                        if (termination_1[0] == -1.0f || termination_1[0] == 1.0f || termination_1[0] == 2.0f)
                        {
                            valid_forward[indices[i + 0]] = 0.0f;
                        }
                        if (termination_1[1] == -1.0f || termination_1[1] == 1.0f || termination_1[1] == 2.0f)
                        {
                            valid_backward[indices[i + 0]] = 0.0f;
                        }
                        if (termination_2[0] == -1.0f || termination_2[0] == 1.0f || termination_2[0] == 2.0f)
                        {
                            valid_forward[indices[i + 1]] = 0.0f;
                        }
                        if (termination_2[1] == -1.0f || termination_2[1] == 1.0f || termination_2[1] == 2.0f)
                        {
                            valid_backward[indices[i + 1]] = 0.0f;
                        }
                        if (termination_3[0] == -1.0f || termination_3[0] == 1.0f || termination_3[0] == 2.0f)
                        {
                            valid_forward[indices[i + 2]] = 0.0f;
                        }
                        if (termination_3[1] == -1.0f || termination_3[1] == 1.0f || termination_3[1] == 2.0f)
                        {
                            valid_backward[indices[i + 2]] = 0.0f;
                        }

                        // Set combination
                        valid_all[indices[i + 0]] = std::min(valid_forward[indices[i + 0]], valid_backward[indices[i + 0]]);
                        valid_all[indices[i + 1]] = std::min(valid_forward[indices[i + 1]], valid_backward[indices[i + 1]]);
                        valid_all[indices[i + 2]] = std::min(valid_forward[indices[i + 2]], valid_backward[indices[i + 2]]);

                        valid_one[indices[i + 0]] = std::max(valid_forward[indices[i + 0]], valid_backward[indices[i + 0]]);
                        valid_one[indices[i + 1]] = std::max(valid_forward[indices[i + 1]], valid_backward[indices[i + 1]]);
                        valid_one[indices[i + 2]] = std::max(valid_forward[indices[i + 2]], valid_backward[indices[i + 2]]);
                    }

                    data_call->set_mask("valid (all)", this->valid_all);
                    data_call->set_mask("valid (one)", this->valid_one);
                    data_call->set_mask("valid (forward)", this->valid_forward);
                    data_call->set_mask("valid (backward)", this->valid_backward);
                }

                // Set new data hash
                data_call->SetDataHash(data_call->DataHash() + 1);

                this->data_output_changed = false;
            }

            // Update transfer functions
            auto set_transfer_function = [](std::shared_ptr<mesh_data_call::data_set> data_set, std::string transfer_function)
            {
                if (data_set != nullptr)
                {
                    std::swap(data_set->transfer_function, transfer_function);

                    data_set->transfer_function_dirty = true;
                }
            };

            if (this->label_transfer_function.IsDirty())
            {
                set_transfer_function(data_call->get_data("labels"), this->label_transfer_function.Param<core::param::TransferFunctionParam>()->Value());
                set_transfer_function(data_call->get_data("labels (forward)"), this->label_transfer_function.Param<core::param::TransferFunctionParam>()->Value());
                set_transfer_function(data_call->get_data("labels (backward)"), this->label_transfer_function.Param<core::param::TransferFunctionParam>()->Value());

                this->label_transfer_function.ResetDirty();
            }

            if (this->distance_transfer_function.IsDirty())
            {
                set_transfer_function(data_call->get_data("distances"), this->distance_transfer_function.Param<core::param::TransferFunctionParam>()->Value());
                set_transfer_function(data_call->get_data("distances (forward)"), this->distance_transfer_function.Param<core::param::TransferFunctionParam>()->Value());
                set_transfer_function(data_call->get_data("distances (backward)"), this->distance_transfer_function.Param<core::param::TransferFunctionParam>()->Value());

                this->distance_transfer_function.ResetDirty();
            }

            if (this->termination_transfer_function.IsDirty())
            {
                set_transfer_function(data_call->get_data("reasons for termination (forward)"), this->termination_transfer_function.Param<core::param::TransferFunctionParam>()->Value());
                set_transfer_function(data_call->get_data("reasons for termination (backward)"), this->termination_transfer_function.Param<core::param::TransferFunctionParam>()->Value());

                this->termination_transfer_function.ResetDirty();
            }

            if (this->gradient_transfer_function.IsDirty())
            {
                set_transfer_function(data_call->get_data("gradients"), this->gradient_transfer_function.Param<core::param::TransferFunctionParam>()->Value());
                set_transfer_function(data_call->get_data("gradients (forward)"), this->gradient_transfer_function.Param<core::param::TransferFunctionParam>()->Value());
                set_transfer_function(data_call->get_data("gradients (backward)"), this->gradient_transfer_function.Param<core::param::TransferFunctionParam>()->Value());

                this->gradient_transfer_function.ResetDirty();
            }

            return true;
        }

        bool implicit_topology::get_data_extent_callback(core::Call& call)
        {
            auto* data_call = dynamic_cast<mesh_data_call*>(&call);
            if (data_call == nullptr) return false;

            data_call->set_data("labels");
            data_call->set_data("labels (forward)");
            data_call->set_data("labels (backward)");

            data_call->set_data("distances");
            data_call->set_data("distances (forward)");
            data_call->set_data("distances (backward)");

            data_call->set_data("reasons for termination (forward)");
            data_call->set_data("reasons for termination (backward)");

            data_call->set_data("gradients");
            data_call->set_data("gradients (forward)");
            data_call->set_data("gradients (backward)");

            data_call->set_mask("valid (all)");
            data_call->set_mask("valid (one)");
            data_call->set_mask("valid (forward)");
            data_call->set_mask("valid (backward)");

            return true;
        }

        bool implicit_topology::get_result_writer_cb_callback(core::Call& call)
        {
            this->get_result_writer_callback = dynamic_cast<implicit_topology_writer_call*>(&call)->GetCallback();

            return true;
        }

        bool implicit_topology::get_screenshot_cb_callback(core::Call& call)
        {
            this->get_screenshot_callback = dynamic_cast<core::view::special::CallbackScreenShooterCall*>(&call)->GetCallback();

            return true;
        }

        bool implicit_topology::get_log_cb_callback(core::Call& call)
        {
            this->get_log_callback = dynamic_cast<core::DirectDataWriterCall*>(&call)->GetCallback();

            return true;
        }

        bool implicit_topology::get_performance_cb_callback(core::Call& call)
        {
            this->get_performance_callback = dynamic_cast<core::DirectDataWriterCall*>(&call)->GetCallback();

            return true;
        }

        bool implicit_topology::start_computation_callback(core::param::ParamSlot& slot)
        {
            // Initialize computation object
            if (!initialize_computation())
            {
                slot.ResetDirty();
                return false;
            }

            // Start computation with current values
            this->computation->start(this->num_integration_steps.Param<core::param::IntParam>()->Value(),
                this->refinement_threshold.Param<core::param::FloatParam>()->Value(),
                this->refine_at_labels.Param<core::param::BoolParam>()->Value(),
                this->distance_difference_threshold.Param<core::param::FloatParam>()->Value(),
                this->num_particles_per_batch.Param<core::param::IntParam>()->Value(),
                this->num_integration_steps_per_batch.Param<core::param::IntParam>()->Value());

            this->last_result = this->computation->get_results();

            this->computation_running = true;

            vislib::sys::Log::DefaultLog.WriteInfo("Computation of topology started...");

            // Set parameters to read-only
            set_readonly_variable_parameters(true);

            return true;
        }

        bool implicit_topology::stop_computation_callback(core::param::ParamSlot&)
        {
            // Terminate computation
            if (this->computation != nullptr && this->computation_running)
            {
                this->computation->terminate();

                vislib::sys::Log::DefaultLog.WriteInfo("Computation of topology terminated!");
            }

            this->computation_running = false;

            // Reset parameters to read-write
            set_readonly_variable_parameters(false);

            return true;
        }

        bool implicit_topology::reset_computation_callback(core::param::ParamSlot& slot)
        {
            // Terminate earlier computation
            stop_computation_callback(slot);

            this->computation = nullptr;
            this->previous_result = nullptr;

            // Reset parameters to read-write
            set_readonly_fixed_parameters(false);
            set_readonly_variable_parameters(false);

            return true;
        }

        bool implicit_topology::load_computation_callback(core::param::ParamSlot& slot)
        {
            // Get load callback
            auto* call = this->result_reader_slot.CallAs<implicit_topology_reader_call>();

            if (call != nullptr && (*call)(0))
            {
                // Reset computation
                reset_computation_callback(slot);

                // Load previous results
                implicit_topology_results previous_results;

                if (!call->GetCallback()(previous_results))
                {
                    slot.ResetDirty();
                    return false;
                }

                // Load input from file
                std::array<unsigned int, 2> resolution;
                std::array<float, 4> domain;

                std::vector<float> positions;
                std::vector<float> vectors;
                std::vector<float> points;
                std::vector<int> point_ids;
                std::vector<float> lines;
                std::vector<int> line_ids;

                if (!load_input(resolution, domain, positions, vectors, points, point_ids, lines, line_ids))
                {
                    slot.ResetDirty();
                    return false;
                }
                    
                // Create new computation object
                this->computation = std::make_unique<implicit_topology_computation>(this->get_log_callback(), this->get_performance_callback(),
                    std::move(resolution), std::move(domain), std::move(positions), std::move(vectors), std::move(points), std::move(point_ids),
                    std::move(lines), std::move(line_ids), previous_results);

                this->integration_timestep.Param<core::param::FloatParam>()->SetValue(previous_results.computation_state.integration_timestep);
                this->max_integration_error.Param<core::param::FloatParam>()->SetValue(previous_results.computation_state.max_integration_error);

                set_readonly_fixed_parameters(true);

                vislib::sys::Log::DefaultLog.WriteInfo("Previous computation of topology loaded from file.");
            }
            else
            {
                vislib::sys::Log::DefaultLog.WriteWarn("Cannot load previous results. Loader module not connected!");
            }

            return true;
        }

        bool implicit_topology::save_computation_callback(core::param::ParamSlot& slot)
        {
            if (this->computation_running)
            {
                vislib::sys::Log::DefaultLog.WriteWarn("Results can only be saved after the computation has finished.");

                slot.ResetDirty();
                return false;
            }

            if (this->previous_result == nullptr)
            {
                vislib::sys::Log::DefaultLog.WriteWarn("There is no result to write to file.");

                slot.ResetDirty();
                return false;
            }

            if (!this->get_result_writer_callback(*this->previous_result))
            {
                slot.ResetDirty();
                return false;
            }

            vislib::sys::Log::DefaultLog.WriteInfo("Previous computation of topology saved to file.");

            return true;
        }
    }
}
