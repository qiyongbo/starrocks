// This file is licensed under the Elastic License 2.0. Copyright 2021 StarRocks Limited.

#pragma once

#include "exec/pipeline/operator.h"
#include "exec/vectorized/aggregator.h"

namespace starrocks::pipeline {
class AggregateDistinctBlockingSinkOperator : public Operator {
public:
    AggregateDistinctBlockingSinkOperator(int32_t id, int32_t plan_node_id, AggregatorPtr aggregator)
            : Operator(id, "aggregate_distinct_blocking_sink", plan_node_id), _aggregator(aggregator) {
        _aggregator->set_aggr_phase(AggrPhase2);
    }
    ~AggregateDistinctBlockingSinkOperator() = default;

    bool has_output() const override { return false; }
    bool need_input() const override { return true; }
    bool is_finished() const override;
    void finish(RuntimeState* state) override;

    Status prepare(RuntimeState* state) override;

    StatusOr<vectorized::ChunkPtr> pull_chunk(RuntimeState* state) override;
    Status push_chunk(RuntimeState* state, const vectorized::ChunkPtr& chunk) override;

private:
    // It is used to perform aggregation algorithms
    // shared by AggregateBlockingSourceOperator
    AggregatorPtr _aggregator = nullptr;
    // Whether prev operator has no output
    bool _is_finished = false;
};

class AggregateDistinctBlockingSinkOperatorFactory final : public OperatorFactory {
public:
    AggregateDistinctBlockingSinkOperatorFactory(int32_t id, int32_t plan_node_id, AggregatorPtr aggregator)
            : OperatorFactory(id, plan_node_id), _aggregator(aggregator) {}

    ~AggregateDistinctBlockingSinkOperatorFactory() override = default;

    OperatorPtr create(int32_t degree_of_parallelism, int32_t driver_sequence) override {
        return std::make_shared<AggregateDistinctBlockingSinkOperator>(_id, _plan_node_id, _aggregator);
    }

private:
    AggregatorPtr _aggregator = nullptr;
};
} // namespace starrocks::pipeline
