unit sr_own_iface_impl_clash;

{ TARGET: declaring the same name in a unit's own interface AND own
  implementation is a duplicate-identifier error. }

interface

const
  K = 1;

implementation

const
  K = 2;

end.
