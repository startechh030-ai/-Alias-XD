# Physics Lab project

`scene.physics` in `project.json` is the whole simulation contract:

| key | default | who reads it |
| --- | --- | --- |
| `enabled` | `true` | the sim loop (step 6) |
| `gravity` | `[0, -9.81, 0]` | step 6 |
| `fixedHz` | `60` | step 6 — the render rate never drives physics |
| `solver` | `builtin` | `builtin`, `jolt`, or `off` |
| `restitution` / `friction` / `density` | `0.15 / 0.4 / 1.0` | per-body defaults for new colliders |

`upAxis` stays `y` here because the box stack is authored that way. Change it to `z` and the
stack falls through the floor — that is the axis contract, not a bug in the solver.

Step 6 decides between the built-in solver and Jolt; until then this project still behaves as
an ordinary scene with the settings already in place.
