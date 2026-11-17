#!/usr/bin/env python3
"""Shared thin 64-bit Mach-O helpers for the iOS and macOS builders."""
import hashlib
import struct

MH_MAGIC_64 = 0xFEEDFACF
LC_LOAD_DYLIB = 0xC
LC_SEGMENT_64 = 0x19
# LC_LOAD_DYLIB plus the weak and upward variants that also carry a dylib path.
DYLIB_PATH_COMMANDS = (LC_LOAD_DYLIB, 0x80000018, 0x8000001F)


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def load_commands(data):
    """Return validated (cmd, offset, size) load commands of a thin 64-bit Mach-O."""
    if len(data) < 32 or struct.unpack_from('<I', data, 0)[0] != MH_MAGIC_64:
        raise ValueError('Expected thin 64-bit Mach-O')
    count, command_size = struct.unpack_from('<II', data, 16)
    end = 32 + command_size
    if end > len(data):
        raise ValueError('Truncated Mach-O load commands')
    commands = []
    cursor = 32
    for _ in range(count):
        if cursor + 8 > end:
            raise ValueError('Truncated Mach-O load command')
        cmd, size = struct.unpack_from('<II', data, cursor)
        if size < 8 or size % 8 or cursor + size > end:
            raise ValueError('Invalid load-command layout')
        commands.append((cmd, cursor, size))
        cursor += size
    if cursor != end:
        raise ValueError('Load command size mismatch')
    return commands


def add_dylib(data, load_path):
    """Add an LC_LOAD_DYLIB for load_path into the zero-filled header padding.

    Returns the patched slice, which keeps its original length, and a record of
    the injection."""
    commands = load_commands(data)
    count, command_size = struct.unpack_from('<II', data, 16)
    cursor = 32 + command_size
    first_section = len(data)
    for cmd, offset, size in commands:
        if cmd in DYLIB_PATH_COMMANDS:
            relative = struct.unpack_from('<I', data, offset + 8)[0]
            name = data[offset + relative:offset + size].split(b'\0')[0]
            if name == load_path.encode():
                raise ValueError('Dylib is already referenced')
        if cmd == LC_SEGMENT_64:
            sections = struct.unpack_from('<I', data, offset + 64)[0]
            for index in range(sections):
                section_offset = struct.unpack_from(
                    '<I', data, offset + 72 + index * 80 + 48)[0]
                if section_offset:
                    first_section = min(first_section, section_offset)
    raw = load_path.encode() + b'\0'
    size = (24 + len(raw) + 7) & ~7
    command = struct.pack('<6I', LC_LOAD_DYLIB, size, 24, 0, 0, 0) + raw
    command += bytes(size - len(command))
    if cursor + size > first_section or any(data[cursor:cursor + size]):
        raise ValueError('Not enough zero-filled header padding; refusing to shift binary data')
    result = bytearray(data)
    struct.pack_into('<II', result, 16, count + 1, command_size + size)
    result[cursor:cursor + size] = command
    return bytes(result), {'load_command_offset': hex(cursor), 'load_command_size': size,
                           'path': load_path, 'first_section_offset': hex(first_section)}
