"""Read-only display eligibility. Unknown or active control state yields to stock UI."""
def eligible(data, fresh):
  if not fresh.get('deviceState', False):
    return False
  ss=data.get('selfdriveState', {});cc=data.get('carControl', {});mads=data.get('selfdriveStateSP', {}).get('mads', {})
  if any((ss.get('enabled'),ss.get('active'),cc.get('enabled'),cc.get('latActive'),cc.get('longActive'),mads.get('enabled'),mads.get('active'))):
    return False
  if ss.get('alertSize', 'none') != 'none' or ss.get('alertText1') or ss.get('alertText2'):
    return False
  if not data['deviceState'].get('started', True):
    return True
  return all(fresh.get(k, False) for k in ('carControl','selfdriveState','selfdriveStateSP','carState')) and ss.get('state')=='disabled' and mads.get('state')=='disabled'
