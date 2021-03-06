/*
 * implicit_topology_computation.h
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VIS).
 * Alle Rechte vorbehalten.
 */
#pragma once

#include "implicit_topology_results.h"
#include "triangulation.h"

#include "../cuda/streamlines.h"

#include <array>
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace megamol
{
    namespace flowvis
    {
        /**
        * Class for computing the implicit topology of a vector field.
        * This computation is performed concurrently, while allowing access to
        * intermediate results from previous computation steps.
        *
        * @author Alexander Straub
        * @author Grzegorz K. Karch
        */
        class implicit_topology_computation
        {
        public:
            /**
            * Initialize computation by providing seed positions and corresponding vectors, convergence structures,
            * and the initial delaunay triangulation of the domain.
            *
            * @param log_stream                         Stream in which to write output
            * @param performance_stream                 Stream in which to write the performance in CSV format
            * @param resolution                         Domain resolution (number of vectors per direction)
            * @param domain                             Domain size (minimum and maximum coordinates)
            * @param positions                          Positions of the vectors, also used as initial seed
            * @param vectors                            Vectors of the vector field
            * @param points                             Convergence structure points (e.g., critical points, periodic orbits, ...)
            * @param point_ids                          Unique IDs (or labels) of the given points
            * @param lines                              Convergence structure lines (e.g., domain boundaries, obstacles, ...)
            * @param line_ids                           (Unique) IDs (or labels) of the given lines
            * @param integration_timestep               (Initial) integration time step
            * @param max_integration_error              Maximum integration error
            * @param method                             Integration method
            */
            implicit_topology_computation(std::ostream& log_stream, std::ostream& performance_stream,
                std::array<unsigned int, 2> resolution, std::array<float, 4> domain,
                std::vector<float> positions, std::vector<float> vectors, std::vector<float> points,
                std::vector<int> point_ids, std::vector<float> lines, std::vector<int> line_ids,
                float integration_timestep, float max_integration_error, streamlines_cuda::integration_method method);

            /**
            * Initialize computation by providing seed positions and corresponding vectors, convergence structures,
            * and the initial delaunay triangulation of the domain. Additionally provide the previous results in order
            * to restart the computation from a different state.
            *
            * @param log_stream                         Stream in which to write output
            * @param performance_stream                 Stream in which to write the performance in CSV format
            * @param resolution                         Domain resolution (number of vectors per direction)
            * @param domain                             Domain size (minimum and maximum coordinates)
            * @param positions                          Positions of the vectors, also used as initial seed
            * @param vectors                            Vectors of the vector field
            * @param points                             Convergence structure points (e.g., critical points, periodic orbits, ...)
            * @param point_ids                          Unique IDs (or labels) of the given points
            * @param lines                              Convergence structure lines (e.g., domain boundaries, obstacles, ...)
            * @param line_ids                           (Unique) IDs (or labels) of the given lines
            * @param previous_result                    Previous results, used as initialization for restarting
            */
            implicit_topology_computation(std::ostream& log_stream, std::ostream& performance_stream,
                std::array<unsigned int, 2> resolution, std::array<float, 4> domain,
                std::vector<float> positions, std::vector<float> vectors, std::vector<float> points,
                std::vector<int> point_ids, std::vector<float> lines, std::vector<int> line_ids,
                implicit_topology_results previous_result);

            /**
            * Destructor
            */
            ~implicit_topology_computation();

            /**
            * Start the computation process.
            *
            * @param num_integration_steps              Number of total integration steps to perform
            * @param refinement_threshold               Threshold for refinement to prevent from refining infinitly
            * @param refine_at_labels                   Refine where different labels meet?
            * @param distance_difference_threshold      Refine when distance difference between neighboring nodes exceed the threshold
            * @param num_particles_per_batch            Number of particles processed and uploaded to the GPU per batch
            * @param num_integration_steps_per_batch    Number of integration steps per batch, after which a new (intermediate) result can be extracted
            */
            void start(unsigned int num_integration_steps, float refinement_threshold, bool refine_at_labels,
                float distance_difference_threshold, unsigned int num_particles_per_batch, unsigned int num_integration_steps_per_batch);

            /**
            * Terminate current computation as soon as possible.
            */
            void terminate();

            /**
            * Get last (intermediate) results.
            *
            * @return Future object on (intermediate) results
            */
            std::shared_future<implicit_topology_results> get_results() const;

        private:
            /**
            * Main algorithm.
            *
            * @param promise                            Promise containing future results
            * @param num_integration_steps              Number of total integration steps to perform
            * @param refinement_threshold               Threshold for refinement to prevent from refining infinitly
            * @param refine_at_labels                   Refine where different labels meet?
            * @param distance_difference_threshold      Refine when distance difference between neighboring nodes exceed the threshold
            * @param num_particles_per_batch            Number of particles processed and uploaded to the GPU per batch
            * @param num_integration_steps_per_batch    Number of integration steps per batch, after which a new (intermediate) result can be extracted
            */
            void run(std::promise<implicit_topology_results>&& promise, unsigned int num_integration_steps, float refinement_threshold,
                bool refine_at_labels, float distance_difference_threshold, unsigned int num_particles_per_batch,
                unsigned int num_integration_steps_per_batch);

            /**
            * Set current results.
            *
            * @param promise    Promise containing future results
            * @param finished   Set finished flag of the results accordingly
            */
            void set_result(std::promise<implicit_topology_results>& promise, bool finished);

            /**
            * Refine the grid around nodes and edges which satisfy the refinement criteria defined by the parameters.
            *
            * @param refinement_threshold               Threshold for refinement to prevent from refining infinitly
            * @param refine_at_labels                   Refine where different labels meet?
            * @param distance_difference_threshold      Refine when distance difference between neighboring nodes exceed the threshold
            *
            * @return Newly created seed points
            */
            std::vector<float> refine_grid(float refinement_threshold, bool refine_at_labels, float distance_difference_threshold);

            /**
            * Output the performance measured
            *
            * @param num_integration_steps              Number of integration steps performed
            */
            void print_performance(unsigned int num_integration_steps) const;

            /** Input domain information */
            const std::array<unsigned int, 2> resolution;
            const std::array<float, 4> domain;

            /** Input seed positions and respective vectors */
            const std::vector<float> positions;
            const std::vector<float> vectors;

            /** Input convergence structures with ids (labels) */
            const std::vector<float> points;
            const std::vector<float> lines;

            const std::vector<int> point_ids;
            const std::vector<int> line_ids;

            /** Input timestep information */
            const float integration_timestep;
            const float max_integration_error;

            /** Integration method */
            streamlines_cuda::integration_method method;

            /** Output positions */
            std::vector<float> positions_forward;
            std::vector<float> positions_backward;

            /** Output labels, distances, and reasons for termination for forward, and backward integration */
            std::vector<float> labels_forward;
            std::vector<float> distances_forward;
            std::vector<float> terminations_forward;

            std::vector<float> labels_backward;
            std::vector<float> distances_backward;
            std::vector<float> terminations_backward;

            /** Number of integration steps performed */
            unsigned int num_integration_steps_performed;

            /** Delaunay triangulation for computing a triangle mesh for refinement */
            triangulation delaunay;

            /** Computation thread */
            std::thread computation;
            bool terminate_computation;

            /** Current results */
            std::shared_future<implicit_topology_results> current_result;

            /** Performance */
            std::size_t performance_num_particles_added;

            using clock_t = std::chrono::high_resolution_clock;
            using duration_t = std::chrono::milliseconds;
            static const char* duration_str;

            duration_t total_runtime;
            duration_t total_time;
            duration_t total_time_integration;
            duration_t total_time_refinement;

            /** Performance output */
            std::ostream& log_output;
            std::ostream& performance_output;
        };
    }
}
