# Execution Engine

`ExecutionEngine` is the workflow runtime boundary. It validates the graph, builds execution indexes, schedules ready nodes through a bounded worker pool, and reports status through `ExecutionEventBus`.

Implemented runtime pieces:

- `GraphValidator`
- `ExecutionEngine`
- `ExecutionEventBus`
- `GraphIndexes`
- `DataPacket`
- `NodeReadinessTracker`
- `InputMerger`
- `WorkerPool`
- `NodeExecutionRequest`
- `NodeExecutionResult`
- `INodeWorker`
- `WorkerRegistry`
- `MockNodeWorker`
- `PythonNodeWorker`

## Validation

`GraphValidator` checks node ids, edge ids, missing node references, invalid ports, cycles, missing Starter nodes, incoming edges on Starter nodes, non-Starter nodes without incoming edges, and nodes unreachable from any Starter.

## Concurrent Execution

`ExecutionEngine::runWorkflow(...)`:

- creates a `runId`
- validates the graph
- builds incoming/outgoing indexes
- starts every ready Starter node
- schedules nodes into `WorkerPool`
- marks join nodes as `Waiting` until all incoming edges are complete
- builds node inputs through `InputMerger`
- executes nodes only through `INodeWorker`
- skips downstream dependencies of failed nodes
- lets unrelated branches continue
- returns `PartiallySucceeded` for mixed success/failure runs

`runWorkflowAsync(...)` wraps the same algorithm in an execution-owned background queue, so UI code does not create ad-hoc run threads.

## Input Merging

Each edge transfers the selected upstream output port. In normal Python nodes this means an edge from `output` sends the upstream node's `outputs["output"]` value.

`InputMerger` builds the downstream `inputs` object by target port:

- One upstream edge to `input`: `inputs["input"]` is that upstream output value.
- Multiple upstream edges to `input`: `inputs["input"]` is a list of upstream output values in workflow edge order.

That lets a join node read merged data like this:

```python
def run(inputs, context):
    items = inputs.get("input", [])
    return {
        "outputs": {
            "output": items
        },
        "artifacts": []
    }
```

## Cancellation

`requestCancelCurrentRun()` marks the active run as cancelled and calls `INodeWorker::cancel(runId)` on each registered worker. `PythonNodeWorker` tracks active `QProcess` instances by `runId`, so cancelling a workflow can terminate blocked Python or Agent code without blocking the Qt UI thread.
