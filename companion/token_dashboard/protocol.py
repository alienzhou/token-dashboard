"""Versioned, bounded snapshot encoding shared with the ESP32 receiver."""
import struct
import zlib
from .collector import DAYS

SERVICE = 'f2d00001-7a43-4d65-a749-b0249cbb1001'
RX = 'f2d00002-7a43-4d65-a749-b0249cbb1001'
ACK = 'f2d00003-7a43-4d65-a749-b0249cbb1001'
PACKET_SIZE = 32 + 5 * 21 + 5 * DAYS * 4 + 4

def encode(snapshot, sequence):
    p = bytearray(struct.pack('<4sIIIIQHH', b'TKD1', sequence, snapshot['updated'],
                  snapshot['end_day'], snapshot['longest_task'], snapshot['peak'],
                  snapshot['longest'], snapshot['current']))
    if len(snapshot['sources']) != 5:
        raise ValueError('expected five sources')
    for s in snapshot['sources']:
        p.extend(struct.pack('<QQHHB', s['total'], s['peak'], s['longest'], s['current'], s['status']))
    for s in snapshot['sources']:
        if len(s['days']) != DAYS:
            raise ValueError('invalid day window')
        p.extend(struct.pack('<' + 'I' * DAYS, *s['days']))
    p.extend(struct.pack('<I', zlib.crc32(p)))
    assert len(p) == PACKET_SIZE
    return bytes(p)

def fragments(packet, payload_size=20):
    if len(packet) != PACKET_SIZE or payload_size <= 6:
        raise ValueError('invalid frame or ATT payload')
    sequence = struct.unpack_from('<I', packet, 4)[0]
    for offset in range(0, len(packet), payload_size - 6):
        yield struct.pack('<IH', sequence, offset) + packet[offset:offset + payload_size - 6]
