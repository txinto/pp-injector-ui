#!/usr/bin/env python3
import csv
import sys
from pathlib import Path
import re

MARKER = "// [PORIS_INTEGRATION_NETVARS]"


def storage_to_enum(storage_type: str) -> str:
    storage_type = (storage_type or "").strip().upper()
    mapping = {
        "BOOL": "NETVARS_TYPE_BOOL",
        "U8": "NETVARS_TYPE_U8",
        "I8": "NETVARS_TYPE_I8",
        "U16": "NETVARS_TYPE_U16",
        "I16": "NETVARS_TYPE_I16",
        "I32": "NETVARS_TYPE_I32",
        "U32": "NETVARS_TYPE_U32",
        "FLOAT": "NETVARS_TYPE_FLOAT",
        "FLOATINT": "NETVARS_TYPE_FLOATINT",
        "STRING": "NETVARS_TYPE_STRING",
    }
    if storage_type not in mapping:
        raise ValueError(f"Unknown storage_type '{storage_type}'")
    return mapping[storage_type]


def nvs_mode_to_enum(mode: str) -> str:
    mode = (mode or "").strip().upper()
    mapping = {
        "NONE": "PRJCFG_NVS_NONE",
        "LOAD": "PRJCFG_NVS_LOAD",
        "SAVE": "PRJCFG_NVS_SAVE",
        "LOADSAVE": "PRJCFG_NVS_LOADSAVE",
        "": "PRJCFG_NVS_NONE",
    }
    if mode not in mapping:
        raise ValueError(f"Unknown nvs_mode '{mode}'")
    return mapping[mode]

def json_repr_to_enum(mode: str) -> str:
    mode = (mode or "").strip().upper()
    mapping = {
        "": "NETVARS_JSON_REPR_AUTO",
        "AUTO": "NETVARS_JSON_REPR_AUTO",
        "ARRAY": "NETVARS_JSON_REPR_ARRAY",
        "HEX": "NETVARS_JSON_REPR_HEX",
        "INVHEX": "NETVARS_JSON_REPR_INVHEX",
        "DEC": "NETVARS_JSON_REPR_DEC",
        "BASE64": "NETVARS_JSON_REPR_BASE64",
    }
    if mode not in mapping:
        raise ValueError(f"Unknown json_repr '{mode}'")
    return mapping[mode]

def json_mode_to_enum(mode: str) -> str:
    mode = (mode or "").strip().upper()
    mapping = {
        "": "NETVARS_JSON_MODE_INOUT",
        "NONE": "NETVARS_JSON_MODE_NONE",
        "OUT": "NETVARS_JSON_MODE_OUT",
        "IN": "NETVARS_JSON_MODE_IN",
        "INOUT": "NETVARS_JSON_MODE_INOUT",
        "APPEND": "NETVARS_JSON_MODE_OUT",
        "PARSE": "NETVARS_JSON_MODE_IN",
    }
    if mode not in mapping:
        raise ValueError(f"Unknown json_mode '{mode}'")
    return mapping[mode]


def read_rows(csv_path: Path):
    with csv_path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        return list(reader)

def canonicalize_rows(rows):
    normalized = []
    for r in rows:
        normalized.append({
            "name": (r.get("name") or "").strip(),
            "c_type": (r.get("c_type") or "").strip(),
            "storage_type": (r.get("storage_type") or "").strip(),
            "nvs_key": (r.get("nvs_key") or "").strip(),
            "json_key": (r.get("json_key") or "").strip(),
            "group": (r.get("group") or "").strip(),
            "module": (r.get("module") or "").strip(),
            "nvs_mode": (r.get("nvs_mode") or "").strip(),
            "json_flag": (r.get("json") or "").strip(),
            "enabler": (r.get("enabler") or "").strip(),
            "json_mode": (r.get("json_mode") or "").strip(),
            "json_repr": (r.get("json_repr") or "").strip(),
            "scale": (r.get("scale") or "").strip(),
        })
    return normalized


def render_types_fragment(rows):
    lines = []
    current_enabler = None
    for r in rows:
        name = r["name"]
        c_type = r["c_type"]
        enabler = r["enabler"]
        if not name or not c_type:
            continue

        if enabler != current_enabler:
            if current_enabler:
                lines.append("#endif")
            if enabler:
                lines.append(f"#ifdef {enabler}")
            current_enabler = enabler

        # Detect arrays like char[LEN] or char [LEN]
        m = re.match(r"^(.*?)(\s*\[.*\])$", c_type)
        if m:
            base = m.group(1).strip()
            array_suffix = m.group(2)
            lines.append(f"    {base} {name}{array_suffix};")
        else:
            lines.append(f"    {c_type} {name};")

    if current_enabler:
        lines.append("#endif")
    return lines


def render_desc_fragment(mod: str, rows):
    data_instance = f"{mod}_dre"

    entries = []
    current_enabler = None
    for r in rows:
        name = r["name"]
        if not name:
            continue

        enabler = r["enabler"]

        storage_type = r["storage_type"]
        nvs_key = r["nvs_key"]
        json_key = r["json_key"]
        group = r["group"]
        module = r["module"]
        nvs_mode = r["nvs_mode"]
        json_flag = r["json_flag"]
        json_mode = r["json_mode"]
        json_repr = r["json_repr"]

        if enabler != current_enabler:
            if current_enabler:
                entries.append("#endif")
            if enabler:
                entries.append(f"#ifdef {enabler}")
            current_enabler = enabler

        type_enum_val = storage_to_enum(storage_type)
        nvs_enum_val = nvs_mode_to_enum(nvs_mode)

        nvs_c = "NULL" if not nvs_key else f"\"{nvs_key}\""
        json_c = "NULL" if not json_key else f"\"{json_key}\""
        group_c = "NULL" if not group else f"\"{group}\""
        module_c = "NULL" if not module else f"\"{module}\""

        json_bool = "true" if json_flag == "1" else "false"

        # Detect arrays like char[LEN] or char [LEN]
        array_len_expr = ""
        m = re.match(r"^(.*?)(\s*\[(.*)\])$", r["c_type"])
        if m:
            array_len_expr = m.group(3).strip()

        if storage_type.upper() == "STRING":
            ptr_expr = f"(void*)({data_instance}.{name})"
        else:
            ptr_expr = f"(void*)&({data_instance}.{name})"

        len_expr = array_len_expr if array_len_expr else "0"
        scale_raw = r.get("scale") or ""
        scale_val = 1
        if scale_raw:
            scale_val = int(scale_raw)
        if storage_type.upper() == "FLOATINT":
            if not scale_raw:
                raise ValueError(f"Missing scale for FLOATINT '{name}'")
            if scale_val <= 0:
                raise ValueError(f"Invalid scale for FLOATINT '{name}': {scale_val}")
        mode_val = json_mode.upper() if isinstance(json_mode, str) else ""
        repr_val = json_repr.upper() if isinstance(json_repr, str) else ""
        json_repr_enum = json_repr_to_enum(repr_val)
        json_mode_enum = json_mode_to_enum(mode_val)

        entry = (
            f'    {{ "{name}", {nvs_c}, {json_c}, {group_c}, {module_c}, '
            f'{type_enum_val}, {nvs_enum_val}, {json_bool}, {json_mode_enum}, {json_repr_enum}, {len_expr}, {ptr_expr}, {scale_val} }},'
        )
        entries.append(entry)

    if current_enabler:
        entries.append("#endif")
    return entries


def apply_marker(out_path: Path, generated_lines, default_header_lines):
    if out_path.exists():
        template_text = out_path.read_text(encoding="utf-8")
    else:
        template_text = "\n".join(default_header_lines + [MARKER]) + "\n"

    if MARKER not in template_text:
        # Fall back to a fresh template if the marker is missing
        template_text = "\n".join(default_header_lines + [MARKER]) + "\n"

    indent = ""
    for line in template_text.splitlines():
        if MARKER in line:
            indent = line.split(MARKER)[0]
            break

    gen_block = "\n".join(indent + ln for ln in generated_lines)
    new_text = template_text.replace(MARKER, gen_block)
    out_path.write_text(new_text.rstrip() + "\n", encoding="utf-8")
    print(f"Generated {out_path}")


def main():
    if len(sys.argv) != 2:
        print("Usage: gen_netvars.py <ModuleName>")
        print("Example: gen_netvars.py PrjCfg")
        sys.exit(1)

    mod = sys.argv[1]  # "PrjCfg", "Measurement", etc.

    root_dir = Path(__file__).resolve().parent.parent
    comp_dir = root_dir / "components" / mod
    if not comp_dir.exists():
        comp_dir = root_dir / "poris" / "components" / mod

    csv_path = comp_dir / "netvars.csv"
    if not csv_path.exists():
        print(f"ERROR: CSV file '{csv_path}' not found")
        sys.exit(1)

    rows = canonicalize_rows(read_rows(csv_path))

    include_dir = comp_dir / "include"
    include_dir.mkdir(parents=True, exist_ok=True)

    fragment_path = include_dir / f"{mod}_netvar_types_fragment.h_"
    c_fragment_path = comp_dir / f"{mod}_netvars_fragment.c_"

    type_header = [
        f"// Auto-generated fragment for {mod} netvars",
        f"// Include this inside the definition of struct {mod}_dre_t",
        "// DO NOT EDIT MANUALLY; edit netvars.csv and regenerate.",
        "",
    ]
    desc_header = [
        f"// Auto-generated fragment for {mod} netvars",
        f"// Included from {mod}_netvars.c",
        "// DO NOT EDIT MANUALLY; edit netvars.csv and regenerate.",
        "",
    ]

    apply_marker(fragment_path, render_types_fragment(rows), type_header)
    apply_marker(c_fragment_path, render_desc_fragment(mod, rows), desc_header)


if __name__ == "__main__":
    main()
