## Human-in-the-Loop (HITL) Comment Lock

`HITL` means human-reviewer-owned comment content.

**Marker standard (code-friendly):**
- Markdown: `<!-- HITL-LOCK START:<id> -->` ... `<!-- HITL-LOCK END:<id> -->`
- C/C++/Java/JS/TS: `// HITL-LOCK START:<id>` ... `// HITL-LOCK END:<id>`
- Python/Shell/YAML/TOML: `# HITL-LOCK START:<id>` ... `# HITL-LOCK END:<id>`

**Rules:**
- AI must never edit, reformat, move, or delete text inside any `HITL-LOCK` block.
- Append-only: AI may add new comments/changes only; prior HITL comments stay unchanged.
- If a locked comment needs revision, add a new note outside the lock or ask the human reviewer to unlock it.

# Lessons Learned — Security Hardening (Phases 0–9)

**Project:** Taktflow Systems
**Completed:** 2026-02-20
**Scope:** 10-phase security hardening from zero to beta-ready

---

## 1. Phase Order Matters

**What worked:** Starting with emergency hardening (Phase 0) before anything else.

| Order | Why |
|-------|-----|
| 0. Emergency fixes first | Stop the bleeding — cron fail-closed, security headers, session hardening |
| 1. Auth & abuse protection | Rate limiting + lockout before you have real users |
| 2. Access controls | RBAC + CSRF after auth is solid |
| 3–4. API + data hardening | Input validation, error messages, PII, dependency audit |
| 5. Monitoring | Can't detect attacks without logging |
| 6. Verification gate | Pentest + checklist BEFORE go-live |
| 7. OAuth + bot protection | Additive features after core is secure |
| 8. Email service | Infrastructure swap (Resend → SMTP) |
| 9. Claim accuracy | Marketing must match code — do this LAST |

**Lesson:** Don't skip to fancy features (OAuth, reCAPTCHA) before the basics are locked down. Phase 0–4 are non-negotiable foundations.

---

## 2. Security Patterns Worth Reusing

### Auth
- **Email normalization**: `trim().toLowerCase()` on every auth endpoint — prevents duplicate accounts
- **Anti-enumeration**: Always return generic success messages ("If an account exists...") on forgot-password
- **Account lockout**: 5 attempts → 15min lock, auto-reset on success (not permanent)
- **Token hygiene**: 32-byte hex, single-use, 1h expiry for reset, 24h for verification
- **Graceful degradation**: Auto-verify email when SMTP not configured (dev environments work without email)

### API
- **Fail-closed crons**: Return 500 if `CRON_SECRET` not set — never silently skip auth
- **Generic error responses**: Never expose `err.message` or stack traces in production
- **Zod on everything**: Input validation at API boundary, not inside business logic
- **ORM-only**: No raw SQL — Drizzle ORM prevents SQL injection by design

### Session
- **JWT + server-side revocation**: JWT for speed, server-side session store for revocation/idle timeout
- **NextAuth v5 naming trap**: Use `isEmailVerified` not `emailVerified` — the latter conflicts with NextAuth's built-in type

### CSRF
- **Origin header validation** in proxy/middleware — simple, effective, no tokens needed
- Apply to all mutation routes (POST/PUT/PATCH/DELETE), not just forms

### Bot Protection
- **Layer reCAPTCHA + honeypot**: reCAPTCHA is bypassable alone; honeypot catches dumb bots; together they cover most cases
- **Graceful degradation**: Skip reCAPTCHA when keys not set (local dev), honeypot always active

---

## 3. Architecture Decisions That Paid Off

### proxy.ts instead of middleware.ts
- Next.js 16 uses `proxy.ts` — having both `middleware.ts` and `proxy.ts` causes build failures
- Centralizing auth + CSRF + security headers in one file is cleaner than scattered middleware

### Compiled .ts files as runtime cache
- Signals (2497), entities (467 T-codes), embeddings (748x384), templates (748) — all compiled to TypeScript
- Zero-DB matching: no database round-trip for the core pipeline
- Import once at build time, serve from memory at runtime

### Structured security logging (pino)
- `component: "security"` tag on all security events
- Searchable in any log aggregator (Datadog, Loki, CloudWatch)
- Events: LOGIN_FAILED, ACCOUNT_LOCKED, RATE_LIMITED, CSRF_REJECTED, etc.

---

## 4. Mistakes and Gotchas

### Resend → SMTP migration
- **Mistake:** Started with Resend (vendor-locked), had to rip it out later
- **Lesson:** Use generic SMTP from day one. nodemailer + any SMTP provider is more portable than any vendor SDK
- **Cleanup required:** References to the old service leak into plans, env examples, health checks — grep thoroughly after any vendor swap

### Marketing claims drift from code
- **Mistake:** Homepage said "no external AI", "every action logged", "230+ T-codes" — none fully accurate
- **Lesson:** Audit marketing claims against code BEFORE launch, not after. Make it a release gate
- **Pattern:** Use qualifiers — "by default", "core processing", "no LLM required" — avoid absolutes like "every", "never", "no"

### Stats go stale
- **Mistake:** Homepage showed 713 templates and 230 T-codes when actual counts were 748 and 467
- **Lesson:** Either pull stats dynamically from compiled data, or add a CI check that compares homepage numbers to compiled file counts

### Dependency audit catches real issues
- **Finding:** `xlsx` package had HIGH severity vulnerability sitting in production dependencies
- **Fix:** Moved to devDependencies (it was only used in build scripts)
- **Lesson:** Run `npm audit --omit=dev` regularly. Move build-only tools to devDependencies

---

## 5. Scaling Prep (TODO:SCALE Pattern)

Instead of building Redis/Upstash from day one, we used in-memory implementations with `TODO:SCALE` markers:

```
// TODO:SCALE — Replace with Redis/Upstash for multi-instance production
const rateLimitStore = new Map<string, RateLimitEntry>();
```

**Where these markers exist:**
- `lib/rate-limit.ts` — in-memory rate limit store → Redis
- `lib/session-manager.ts` — in-memory session cache → Redis

**Why this works for beta:**
- Single Vercel instance = in-memory is fine
- Markers are greppable: `grep -r "TODO:SCALE"` finds all migration points
- Each marker includes the target solution (Upstash/Redis)

**When to migrate:** Before scaling to multiple instances or when rate limits reset on cold starts become a problem.

---

## 6. Pentest Script > Manual Testing

- Automated pentest script covered 29 tests in seconds
- Manual tests (cross-org access, unverified email) still needed for edge cases requiring multiple accounts
- **Lesson:** Automate everything you can. Keep a short list of manual-only tests that need real accounts/state

---

## 7. Health Endpoint Checklist

A good `/api/health` endpoint checks:
- Database connectivity (query succeeds)
- Required env vars present (not their values — just existence)
- NODE_ENV matches expected (production vs development)
- Returns structured JSON with status codes (200 healthy, 503 degraded)

---

## 8. Security Headers Baseline

Copy-paste for any Next.js project:

```
X-Frame-Options: DENY
X-Content-Type-Options: nosniff
Referrer-Policy: strict-origin-when-cross-origin
Strict-Transport-Security: max-age=63072000; includeSubDomains; preload
Permissions-Policy: camera=(), microphone=(), geolocation=()
Content-Security-Policy: default-src 'self'; script-src 'self' ...; frame-ancestors 'none'
```

---

## 9. Process Lessons

- **Always write a plan before implementing** — prevents wasted work and misalignment
- **Always update the plan before starting the next phase** — keeps the master doc as source of truth
- **Phase 9 (claim accuracy) should be a release gate** — validate every public-facing claim against actual code before every deploy
- **Vendor swaps require full grep cleanup** — old vendor names leak into plans, env examples, health checks, comments
- **Use qualifiers in marketing** — "by default", "core processing", "configurable" — never "every", "never", "always"

---

## 10. Reusable File Structure

```
lib/
  api-auth.ts        # RBAC helper (requireRole, requireAuth)
  audit.ts           # Audit logger (logAudit with sanitization)
  csrf.ts            # CSRF origin validation
  email.ts           # SMTP email (nodemailer, graceful degradation)
  rate-limit.ts      # Per-IP + per-key rate limiter
  recaptcha.ts       # Server-side reCAPTCHA verification
  recaptcha-client.ts # Client-side useRecaptcha() hook
  redaction.ts       # PII redaction for logs
  security-log.ts    # Structured security event logger (pino)
  session-manager.ts # Server-side session store + revocation
  validations.ts     # Zod schemas for all API inputs
proxy.ts             # Auth + CSRF + headers (Next.js 16 middleware)
auth.ts              # NextAuth v5 config (JWT, providers, callbacks)
```

---

## Quick Reference: Security Stack

| Layer | Implementation | Effort |
|-------|---------------|--------|
| Auth | NextAuth v5 JWT + bcrypt | 1 day |
| Rate limiting | In-memory Map (→ Redis) | 2 hours |
| Account lockout | DB columns + check in auth | 2 hours |
| CSRF | Origin header in proxy | 1 hour |
| RBAC | Helper function + role check | 2 hours |
| Org isolation | WHERE clause in every query | Ongoing |
| Input validation | Zod schemas | Ongoing |
| XSS | React auto-escaping (free) | 0 |
| reCAPTCHA v3 | Client hook + server verify | 3 hours |
| Honeypot | Hidden form field | 30 min |
| Security headers | next.config.ts headers | 1 hour |
| Security logging | pino structured logger | 2 hours |
| Error hardening | Generic messages, no traces | 1 hour |
| Email | nodemailer SMTP | 2 hours |
| Claim accuracy audit | Grep + manual review | 2 hours |
| Automated pentest | Script testing all endpoints | 3 hours |

**Total estimated effort for full stack: ~3-4 days**

