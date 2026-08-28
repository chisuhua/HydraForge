### AgenticDSL /__meta__
version: "1.0"
mode: cost_optimization

### AgenticDSL /cross_cutting
patterns:
  - type: decorator-v1
    config:
      decorators: ["CostTracking"]
  - type: bus-v1
    config:
      subscriptions: ["tool.completed", "llm.response"]
