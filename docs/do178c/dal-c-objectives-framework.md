# DO-178C DAL C process framework

This document describes the DO-178C Design Assurance Level C objectives and
lifecycle data this project is structured to eventually satisfy, and how
the repository maps onto them. **This is a process framework, not a
certification submission.** Actual certification requires engagement with
an applicant's certification liaison process and the certifying authority
(FAA/EASA/etc.) via a DER or equivalent, plus program-specific PSAC
agreement — none of that is performed here.

## What DAL C requires (and doesn't)

DO-178C ties objectives to Design Assurance Level via Annex A tables. The
points most relevant to how this codebase is structured:

- **Structural coverage (Table A-7)**: DAL C requires **Statement
  Coverage**. It does *not* require Decision Coverage (DAL B) or Modified
  Condition/Decision Coverage — MC/DC (DAL A). This project's test/coverage
  tooling targets statement coverage as the bar, while writing tests
  thoroughly enough that decision-level coverage is a natural byproduct
  where cheap — it costs little and reduces rework if a component is later
  reused at a higher DAL.
- **Independence**: DAL C requires independence for certain verification
  objectives (e.g., some review and test activities) but less pervasively
  than DAL A/B. This affects process (who signs off what), not the
  repository structure, and is out of scope for this codebase-level plan.
- **Source-to-object code traceability**: only required at DAL A. Not
  targeted here.
- **Low-level requirements and derived requirements**: still required at
  DAL C. The requirements-based design and test approach below applies.

## Lifecycle data this project plans for

| Artifact | Purpose | Where it lives (planned) |
|---|---|---|
| PSAC (Plan for Software Aspects of Certification) | Program-level cert plan, agreed with the certification authority | Not produced by this repo — applicant-owned, program-specific |
| SDP (Software Development Plan) | How software is developed | `docs/do178c/sdp.md` (roadmap) |
| SVP (Software Verification Plan) | How software is verified | `docs/do178c/svp.md` (roadmap) |
| SCMP (Software Configuration Management Plan) | Baselines, change control, problem reporting | `docs/do178c/scmp.md` (roadmap); in practice, git history + tags + this docs tree |
| SQAP (Software Quality Assurance Plan) | QA process, conformance audits | `docs/do178c/sqap.md` (roadmap) |
| Software Requirements Data | High- and low-level requirements | `docs/do178c/requirements/` (roadmap), one file per component, traced to design |
| Software Design Data | Architecture and detailed design | `docs/architecture/` ADRs + component-level design notes |
| Source Code | The implementation | `runtime/`, `idlc/` |
| Verification Cases, Procedures, Results | Test cases tied to requirements, and their results | `runtime/tests/`, `idlc/tests/`, with traceability comments/IDs back to requirements |
| Traceability Data | Requirement → design → code → test mapping | `docs/do178c/traceability.md` (roadmap) or a generated matrix once requirements exist |
| Problem Reports | Defect tracking | Issue tracker (external to this repo) |
| Software Configuration Index / Accomplishment Summary | End-of-effort summary artifacts | `docs/do178c/` (roadmap, produced near a certification milestone, not now) |

## Coding standard

C code in `runtime/` follows a MISRA C:2012 subset (mandatory + required
rules as a floor; advisory rules adopted where they don't conflict with
the no-dynamic-allocation memory model). The specific rule set and any
documented deviations will be recorded in `docs/do178c/coding-standard.md`
once static analysis tooling is wired into CI (see `docs/roadmap.md`).

## Tool qualification

`idlc` (the IDL compiler) generates source that feeds the certified build.
For now, generated code is verified exactly like hand-written code — full
review, requirements-based testing, and statement coverage apply to it.
This means `idlc` itself does **not** need DO-330 tool qualification yet.
Pursuing DO-330 Tool Qualification Level (TQL) qualification of `idlc` (to
reduce downstream verification burden on generated code) is a future
roadmap item — see `docs/roadmap.md`.
