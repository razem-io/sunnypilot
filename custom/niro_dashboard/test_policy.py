import unittest
from policy import eligible
class PolicyTests(unittest.TestCase):
  def setUp(self):
    self.d={'deviceState':{'started':True},'selfdriveState':{'state':'disabled'},'selfdriveStateSP':{'mads':{'state':'disabled'}},'carControl':{},'carState':{}}
    self.f={k:True for k in self.d}
  def test_manual(self):self.assertTrue(eligible(self.d,self.f))
  def test_all_unknown_inputs_fail_closed_onroad(self):
    for k in self.f:
      with self.subTest(k=k):self.assertFalse(eligible(self.d,{**self.f,k:False}))
  def test_controls_and_mads(self):
    for topic,key in [('carControl','latActive'),('carControl','longActive'),('carControl','enabled'),('selfdriveState','enabled'),('selfdriveState','active')]:
      with self.subTest(topic=topic,key=key):
        self.d[topic][key]=True;self.assertFalse(eligible(self.d,self.f));self.d[topic][key]=False
    self.d['selfdriveStateSP']['mads']['active']=True;self.assertFalse(eligible(self.d,self.f))
  def test_alert(self):
    self.d['selfdriveState']['alertSize']='small';self.assertFalse(eligible(self.d,self.f))
  def test_offroad(self):
    self.d['deviceState']['started']=False;self.assertTrue(eligible(self.d,{'deviceState':True}));self.assertFalse(eligible(self.d,{}))
if __name__=='__main__':unittest.main()
