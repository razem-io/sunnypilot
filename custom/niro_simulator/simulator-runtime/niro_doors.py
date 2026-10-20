"""Read-only first-generation Niro door decoder, Hyundai generic DBC.

Call only for a positively identified KIA NIRO EV (non-CAN-FD), main CAN bus.
CarState.doorOpen alone is NOT sufficient. No transmit API or inferred doors.
"""
DOORS = {'frontLeft': (1345, 8, 2), 'frontRight': (1345, 35, 1),
         'rearLeft': (1363, 24, 1), 'rearRight': (1363, 23, 1)}

def decode_doors(frames, *, niro_ev_verified, now, max_age=.5):
    """frames: {CAN address: (8-byte payload, monotonic receive time)}.

    Driver values 2/3 are reserved/ambiguous: retain raw, expose unknown.
    The stock parser uses truthiness; confirm those states on real hardware.
    """
    result = {name: None for name in DOORS}
    raw = {}
    if not niro_ev_verified:
        return {'doors': result, 'raw': raw, 'complete': False}
    for name, (address, shift, width) in DOORS.items():
        frame = frames.get(address)
        if frame is None:
            continue
        payload, received = frame
        if len(payload) != 8 or not 0 <= now - received <= max_age:
            continue
        value = (int.from_bytes(payload, 'little') >> shift) & ((1 << width) - 1)
        raw[name] = value
        if value in (0, 1):
            result[name] = bool(value)
    return {'doors': result, 'raw': raw, 'complete': all(v is not None for v in result.values())}
