# SPDX-License-Identifier: GPL-2.0-or-later
"""zrn init-agents: tell coding agents how to build this app.

One source text (templates/agents/INSTRUCTIONS.md) becomes each agent's
instruction file, and one skill (templates/agents/SKILL.md, name: zrnui)
goes where each agent looks for skills. Where an agent's conventions change,
change AGENTS below; nothing else knows the paths.
"""
import json
import sys
from pathlib import Path

from .errors import ZrnError

TEMPLATES = Path(__file__).resolve().parents[2] / "templates" / "agents"

# name: (label, instruction files, skill folders, notes). Paths are relative
# to the project. A .mdc file gets Cursor's frontmatter.
AGENTS = {
    "claude": ("Claude Code", ["CLAUDE.md"], [".claude/skills/zrnui"], "reads CLAUDE.md; skills in .claude/skills"),
    "codex": ("OpenAI Codex", ["AGENTS.md"], [".agents/skills/zrnui"], "reads AGENTS.md; skills in .agents/skills"),
    "gemini": ("Gemini CLI", ["GEMINI.md"], [".gemini/skills/zrnui"], "reads GEMINI.md; skills in .gemini/skills"),
    "cursor": ("Cursor", [".cursor/rules/zrnui.mdc"], [".cursor/skills/zrnui"],
               "project rules in .cursor/rules (also reads AGENTS.md)"),
    "copilot": ("GitHub Copilot", [".github/copilot-instructions.md"], [".github/skills/zrnui"],
                "repository instructions; agent skills in .github/skills"),
    "opencode": ("OpenCode", ["AGENTS.md"], [".opencode/skills/zrnui"], "reads AGENTS.md; skills in .opencode/skills"),
    "antigravity": ("Antigravity", [".agent/rules/zrnui.md"], [".agent/skills/zrnui"],
                    "workspace rules in .agent/rules; skills in .agent/skills"),
    "windsurf": ("Windsurf", [".windsurf/rules/zrnui.md"], [".windsurf/skills/zrnui"],
                 "workspace rules in .windsurf/rules (also reads AGENTS.md)"),
}
AGENT_NAMES = list(AGENTS)


def _instruction_text(path):
    text = (TEMPLATES / "INSTRUCTIONS.md").read_text()
    if path.endswith(".mdc"):
        return ("---\ndescription: ztOS app rules (libzrnui, zrnCoolStyle, the zrn CLI)\n"
                "alwaysApply: true\n---\n\n" + text)
    if path.startswith((".windsurf/", ".agent/")):
        return "---\ntrigger: always_on\n---\n\n" + text
    return text


def _pick_interactively():
    names = AGENT_NAMES
    print("Which coding agents should know how to build this app?", file=sys.stderr)
    for i, n in enumerate(names, 1):
        print(f"  {i}. {AGENTS[n][0]}", file=sys.stderr)
    print("Numbers separated by commas, 'a' for all, Enter for none: ", end="", file=sys.stderr, flush=True)
    line = sys.stdin.readline().strip().lower()
    if not line:
        return []
    if line in ("a", "all"):
        return names
    picked = []
    for part in line.replace(" ", "").split(","):
        if part.isdigit() and 1 <= int(part) <= len(names):
            picked.append(names[int(part) - 1])
        elif part in AGENTS:
            picked.append(part)
        else:
            raise ZrnError("bad-choice", f"{part!r} is not one of the numbers shown", "run it again")
    return picked


def init_agents(args, out):
    from .cli import find_project, result
    if args.list:
        rows = [{"agent": n, "name": a[0], "instructions": a[1], "skills": a[2], "notes": a[3]}
                for n, a in AGENTS.items()]
        if out.json:
            print(json.dumps({"ok": True, "command": "init-agents", "agents": rows}, indent=1))
        else:
            for r in rows:
                print(f"{r['agent']:<12} {', '.join(r['instructions']):<36} {', '.join(r['skills'])}")
        return 0
    # only ever into an app's own folder (one with zrnapp.toml)
    root = find_project(args.project).root
    if args.all:
        chosen = AGENT_NAMES
    elif args.agent:
        chosen = [a.strip().lower() for a in args.agent.split(",") if a.strip()]
        bad = [a for a in chosen if a not in AGENTS]
        if bad:
            raise ZrnError("unknown-agent", f"unknown agent: {', '.join(bad)}",
                           f"choose from: {', '.join(AGENT_NAMES)} (or --all)", exit_code=3)
    elif sys.stdin.isatty() and not out.json:
        chosen = _pick_interactively()
    else:
        raise ZrnError("no-agent", "no agent chosen and no terminal to ask in",
                       f"pass --agent {','.join(AGENT_NAMES[:2])} or --all", exit_code=3)
    skill = (TEMPLATES / "SKILL.md").read_text()
    written, kept = [], []
    for name in chosen:
        _, instr, skills, _ = AGENTS[name]
        for rel in instr:
            target = root / rel
            if target.exists() and not args.force and target.read_text() != _instruction_text(rel):
                kept.append(rel)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(_instruction_text(rel))
            written.append(rel)
        for rel in skills:
            target = root / rel / "SKILL.md"
            if target.exists() and not args.force and target.read_text() != skill:
                kept.append(str(Path(rel) / "SKILL.md"))
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(skill)
            written.append(str(Path(rel) / "SKILL.md"))
    written = sorted(set(written))
    result(out, {"command": "init-agents", "path": str(root), "agents": chosen, "written": written,
                 "kept": kept},
           "\n".join([f"wrote {w}" for w in written] + [f"kept {k} (changed by hand; --force replaces it)"
                                                         for k in kept]) or "nothing chosen")
    return 0
