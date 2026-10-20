"""Read-only display conversion; never replace invalid live values with zero."""
import math

def finite(value):
  return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)

def readings(car, fresh, ratio=None):
  live = bool(fresh and car.get('canValid', False))
  speed = car.get('vEgo')
  steering = car.get('steeringAngleDeg')
  speed = round(speed * 3.6) if live and finite(speed) else None
  steering = round(steering, 1) if live and finite(steering) else None
  wheel = round(steering / ratio, 2) if steering is not None and finite(ratio) and 1 < ratio < 100 else None
  return {'live': live, 'speed': speed, 'steering': steering, 'wheelAngle': wheel}
