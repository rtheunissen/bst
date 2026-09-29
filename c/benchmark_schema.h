#ifndef BENCHMARK_SCHEMA_H
#define BENCHMARK_SCHEMA_H

enum {
   BENCHMARK_SCHEMA_BALANCER_MEASUREMENT_FIELDS = 10,
   BENCHMARK_SCHEMA_BALANCER_BENCHMARK_FIELDS = 6,
   BENCHMARK_SCHEMA_OPERATION_BENCHMARK_FIELDS = 8,
   BENCHMARK_SCHEMA_OPERATION_MEASUREMENT_FIELDS = 12,
};

#define BENCHMARK_SCHEMA_BALANCER_MEASUREMENT_HEADER                           \
   "Distribution Scale Size Iteration PartitionCount PartitionDepth "          \
   "AveragePathLength MaximumPathLength Rotations Allocations"

#define BENCHMARK_SCHEMA_BALANCER_BENCHMARK_HEADER                             \
   "Distribution Scale Size Iterations AverageDuration TotalDuration"

#define BENCHMARK_SCHEMA_OPERATION_BENCHMARK_HEADER                            \
   "Distribution Scale Steps Size Step Position Iteration Duration"

#define BENCHMARK_SCHEMA_OPERATION_MEASUREMENT_HEADER                          \
   "Distribution Scale Steps Size Step Position PartitionCount "               \
   "PartitionDepth "                                                           \
   "AveragePathLength MaximumPathLength Rotations Allocations"

#endif // BENCHMARK_SCHEMA_H
