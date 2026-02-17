# Bugame Release Variants

Each variant has its own `project.mk` with its own `Name`, Android package, and content root.

Variants:
- `chaos/project.mk`
- `angrybirds/project.mk`
- `candycrash/project.mk`

Build examples:

```bash
# Chaos Cars
./builder/bin/builder build projects/bugame/releases/chaos web --run --detach
./builder/bin/builder build projects/bugame/releases/chaos android --run

# Angry Birds
./builder/bin/builder build projects/bugame/releases/angrybirds web --run --detach
./builder/bin/builder build projects/bugame/releases/angrybirds android --run

# CandyCrash
./builder/bin/builder build projects/bugame/releases/candycrash web --run --detach
./builder/bin/builder build projects/bugame/releases/candycrash android --run
```

Each variant packages only files inside its own content root:
- `releases/<variant>/scripts`
- `releases/<variant>/assets`
- optional: `resources`, `data`, `media`
