# mdcat demo

Some **bold**, *italic*, ~~strike~~, `inline code` and a [link](https://example.com). A very long line of text that should wrap nicely at the terminal width so we can verify the word wrapper handles things properly.

## Lists

- one
- two
  - nested a
  - nested b
- [x] done task
- [ ] open task

1. first
2. second

> A quote with **bold** text
> > nested quote

| Name | Qty | Notes |
|:-----|----:|:-----:|
| apple | 3 | fresh |
| banana | 12 | ripe and yellow |

---

```python
print("hi")
```

```mermaid
flowchart TD
    A[Start] --> B{Is it working?}
    B -- Yes --> C[Ship it]
    B -- No --> D[Debug]
    D --> B
    C --> E((Done))
```

```mermaid
graph LR
    A[Client] -->|request| B(Server)
    B --> C[(Database)]
    B --> D[Cache]
    C --> E[Disk]
```

```mermaid
sequenceDiagram
    participant A as Alice
    participant B as Bob
    A->>B: Hello Bob
    B-->>A: Hi Alice
    Note right of B: thinking
    A->>A: self call
```

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Running : start
    Running --> Idle : stop
    Running --> [*]
```

```mermaid
pie title Pets
    "Dogs" : 40
    "Cats" : 35
    "Fish" : 25
```

```mermaid
classDiagram
  A <|-- B
```
