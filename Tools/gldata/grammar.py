"""Id and tag grammar (Docs/CONTENT-IDS-AND-TAGS.md sections 1 and 4)."""

from __future__ import annotations

import re

SEGMENT = re.compile(r"^[a-z][a-z0-9_]{0,31}$")
TAG_PART = re.compile(r"^[A-Z][A-Za-z0-9]{0,31}$")

ID_MIN_SEGMENTS = 3
ID_MAX_SEGMENTS = 5
ID_MAX_LENGTH = 96
TAG_MAX_PARTS = 4
TAG_MAX_LENGTH = 96


def id_problem(value: object) -> str | None:
    """Why `value` is not a well-formed id (ID-1), or None if it is."""
    if not isinstance(value, str):
        return "id must be a string"
    if len(value) > ID_MAX_LENGTH:
        return f"id longer than {ID_MAX_LENGTH} characters"
    segments = value.split(".")
    if not ID_MIN_SEGMENTS <= len(segments) <= ID_MAX_SEGMENTS:
        return f"id needs {ID_MIN_SEGMENTS}-{ID_MAX_SEGMENTS} dot-separated segments, has {len(segments)}"
    for segment in segments:
        if not SEGMENT.match(segment):
            return f"segment '{segment}' must match [a-z][a-z0-9_]{{0,31}}"
        if "__" in segment or segment.endswith("_"):
            return f"segment '{segment}' must not contain '__' or end with '_'"
    return None


def id_kind(value: str) -> str:
    return value.split(".", 1)[0]


def tag_problem(value: object) -> str | None:
    """Why `value` is not a well-formed tag (TAG-1), or None if it is."""
    if not isinstance(value, str):
        return "tag must be a string"
    if len(value) > TAG_MAX_LENGTH:
        return f"tag longer than {TAG_MAX_LENGTH} characters"
    parts = value.split(".")
    if len(parts) > TAG_MAX_PARTS:
        return f"tag has more than {TAG_MAX_PARTS} parts"
    for part in parts:
        if not TAG_PART.match(part):
            return f"tag part '{part}' must be PascalCase [A-Z][A-Za-z0-9]{{0,31}}"
    return None


def tag_namespace(value: str) -> str:
    return value.split(".", 1)[0]
