#!/usr/bin/env python3
"""
Helper to generate integration snippets for app_main.c

Two modes:
- Default: print snippets to stdout (manual insertion).
- --apply: insert snippets into main/app_main.c before placeholder markers:
  // [PORIS_INTEGRATION_INCLUDE]
  // [PORIS_INTEGRATION_INIT]
  // [PORIS_INTEGRATION_START]
  // [PORIS_INTEGRATION_DEFINES]
  // [PORIS_INTEGRATION_COUNTERS]
  // [PORIS_INTEGRATION_RUN]
  // [PORIS_INTEGRATION_NETVARS_INCLUDE]
  // [PORIS_INTEGRATION_NETVARS_PARSE]
  // [PORIS_INTEGRATION_NETVARS_APPEND]

Example:
  python3 poris/add_component_integration.py \
      --name DualLED \
      --enable-macro CONFIG_PORIS_ENABLE_DUALLED \
      --include "<DualLED.h>" \
      --use-thread-macro CONFIG_DUALLED_USE_THREAD \
      --spin-period-ms 100 \
      --apply
"""

import argparse
from pathlib import Path
import sys


def main():
    ap = argparse.ArgumentParser(description="Generate app_main integration snippets for a component.")
    ap.add_argument("--name", required=True, help="Component name (e.g., DualLED)")
    ap.add_argument("--enable-macro", help="Kconfig macro that enables the component (default: CONFIG_PORIS_ENABLE_<NAME>)")
    ap.add_argument("--include", help="Header to include, e.g. <DualLED.h> (default: <Name.h>)")
    ap.add_argument("--netvars-include", help="Netvars header to include, e.g. <DualLED_netvars.h> (default: <Name_netvars.h>)")
    ap.add_argument("--use-thread-macro", help="Macro that indicates threaded mode (default: CONFIG_<NAME>_USE_THREAD). If omitted, no start()/spin branching is generated.")
    ap.add_argument("--spin-period-ms", type=int, default=100, help="Spin period in ms. Set 0 or omit --use-thread-macro to skip spin snippet.")
    ap.add_argument("--period-macro", help="Macro name for the spin period (default derived from name).")
    ap.add_argument("--counter-name", help="Counter variable name for spin (default derived from name).")
    ap.add_argument("--no-spin", action="store_true", help="Do not generate spin/counter snippets.")
    ap.add_argument("--type-library", action="store_true", help="Library component: only include and setup snippets.")
    ap.add_argument("--apply", action="store_true", help="Insert snippets into main/app_main.c using markers.")
    ap.add_argument("--app-main", default="main/app_main.c", help="Path to app_main.c (for --apply).")
    args = ap.parse_args()

    name = args.name
    def upper_s(s): return s.upper()
    en_macro = args.enable_macro or f"CONFIG_PORIS_ENABLE_{upper_s(name)}"
    include = args.include or f"<{name}.h>"
    netvars_include = args.netvars_include or f"<{name}_netvars.h>"
    use_thread_macro = args.use_thread_macro or f"CONFIG_{upper_s(name)}_USE_THREAD"
    gen_spin = not args.no_spin and use_thread_macro and args.spin_period_ms > 0

    if not args.period_macro:
        args.period_macro = f"{upper_s(name)}_CYCLE_PERIOD_MS"
    if not args.counter_name:
        args.counter_name = f"{name.lower()}_cycle_counter"
    limit_macro = args.period_macro.replace("PERIOD", "LIMIT")

    snippets = {
        "include": "\n".join([
            f"#ifdef {en_macro}",
            f"#include {include}",
            "#endif",
            ""
        ]),
        "init": "\n".join([
            f"#ifdef {en_macro}",
            f'    error_occurred = ({name}_setup() != {name}_ret_ok);',
            "    error_accumulator |= error_occurred;",
            "#endif",
            ""
        ]),
        "start": "\n".join([
            f"#ifdef {en_macro}",
            f'    error_occurred = ({name}_enable() != {name}_ret_ok);',
            (f"#ifdef {use_thread_macro}\n"
             "    if (!error_occurred)\n"
             "    {\n"
             f"        error_occurred |= ({name}_start() != {name}_ret_ok);\n"
             "    }\n"
             "#endif") if use_thread_macro else "",
            "    error_accumulator |= error_occurred;",
            "#endif",
            ""
        ]),
        "netvars_include": "\n".join([
            f"#ifdef {en_macro}",
            f"#include {netvars_include}",
            "#endif",
            ""
        ]),
        "netvars_parse": "\n".join([
            f"#ifdef {en_macro}",
            f"    {name}_config_parse_json(data);",
            "#endif",
            ""
        ]),
        "netvars_append": "\n".join([
            f"#ifdef {en_macro}",
            f"    {name}_netvars_append_json(root);",
            "#endif",
            ""
        ]),
    }

    if gen_spin:
        defines_lines = [
            f"#define {args.period_macro} {args.spin_period_ms}",
            f"#define {limit_macro} (({args.period_macro} / MAIN_CYCLE_PERIOD_MS) - 1)",
        ]
        counters_lines = [
            f"static uint8_t {args.counter_name} = 0;",
        ]
        if use_thread_macro:
            snippets["defines"] = "\n".join([
                f"#ifndef {use_thread_macro}",
                *defines_lines,
                "#endif",
                ""
            ])
            snippets["counters"] = "\n".join([
                f"#ifndef {use_thread_macro}",
                *counters_lines,
                "#endif",
                ""
            ])
        else:
            snippets["defines"] = "\n".join(defines_lines + [""])
            snippets["counters"] = "\n".join(counters_lines + [""])
        snippets["run"] = "\n".join([
            f"#ifdef {en_macro}",
            f"#ifndef {use_thread_macro}",
            f"    if ({args.counter_name} <= 0)",
            "    {",
            f"        error_accumulator |= ({name}_spin() != {name}_ret_ok);",
            f"        {args.counter_name} = {limit_macro};",
            "    }",
            "    else",
            "    {",
            f"        {args.counter_name}--;",
            "    }",
            "#endif",
            "#endif",
            ""
        ])
    else:
        snippets["defines"] = ""
        snippets["counters"] = ""
        snippets["run"] = ""

    if args.type_library:
        snippets["start"] = ""
        snippets["defines"] = ""
        snippets["counters"] = ""
        snippets["run"] = ""
        snippets["netvars_include"] = ""
        snippets["netvars_parse"] = ""
        snippets["netvars_append"] = ""

    if not args.apply:
        for key, val in snippets.items():
            if val:
                print(f"=== {key} ===")
                print(val)
        return

    # Apply mode
    markers = {
        "include": "// [PORIS_INTEGRATION_INCLUDE]",
        "init": "// [PORIS_INTEGRATION_INIT]",
        "start": "// [PORIS_INTEGRATION_START]",
        "defines": "// [PORIS_INTEGRATION_DEFINES]",
        "counters": "// [PORIS_INTEGRATION_COUNTERS]",
        "run": "// [PORIS_INTEGRATION_RUN]",
        "netvars_include": "// [PORIS_INTEGRATION_NETVARS_INCLUDE]",
        "netvars_parse": "// [PORIS_INTEGRATION_NETVARS_PARSE]",
        "netvars_append": "// [PORIS_INTEGRATION_NETVARS_APPEND]",
    }

    app_path = Path(args.app_main)
    if not app_path.exists():
        print(f"app_main not found: {app_path}", file=sys.stderr)
        sys.exit(2)
    text = app_path.read_text(encoding="utf-8")

    def insert_before_marker(txt: str, marker: str, snippet: str) -> str:
        if not snippet.strip():
            return txt
        if marker not in txt:
            raise ValueError(f"Marker not found: {marker}")
        if snippet.strip() in txt:
            return txt  # avoid duplicate
        parts = txt.split(marker)
        return parts[0] + snippet + marker + marker.join(parts[1:])

    try:
        text = insert_before_marker(text, markers["include"], snippets["include"])
        text = insert_before_marker(text, markers["init"], snippets["init"])
        text = insert_before_marker(text, markers["start"], snippets["start"])
        text = insert_before_marker(text, markers["defines"], snippets["defines"])
        text = insert_before_marker(text, markers["counters"], snippets["counters"])
        text = insert_before_marker(text, markers["run"], snippets["run"])
        text = insert_before_marker(text, markers["netvars_include"], snippets["netvars_include"])
        text = insert_before_marker(text, markers["netvars_parse"], snippets["netvars_parse"])
        text = insert_before_marker(text, markers["netvars_append"], snippets["netvars_append"])
    except ValueError as e:
        print(str(e), file=sys.stderr)
        sys.exit(3)

    app_path.write_text(text, encoding="utf-8")
    print(f"Inserted snippets for {name} into {app_path}")


if __name__ == "__main__":
    main()
