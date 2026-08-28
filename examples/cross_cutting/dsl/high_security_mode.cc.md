### AgenticDSL /__meta__
version: "1.0"
mode: high_security

### AgenticDSL /cross_cutting
patterns:
  - type: decorator-v1
    config:
      decorators: ["CostTracking", "Compliance", "PII-Scrub"]
  - type: hook-v1
    config:
      hooks:
        - target: tool
          glob: "L3_*"
          type: pre
          priority: 1000
          policy: FailClosed
          handler: human-approval-v1
  - type: composition-v1
    config:
      agents:
        - name: privacy-policy-v1
          scope: "react-loop/*"
  - type: bus-v1
    config:
      subscriptions: ["mutation.committed"]
      handler: external-siem-adapter-v1
