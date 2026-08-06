unit sr_iface_cannot_see_impl_uses;

{ TARGET rule 2f: the interface section may only see symbols from units in
  its own `uses` clause (interface-uses). A symbol brought in only by the
  implementation `uses` clause must not be visible here. sr_impl_const is in
  the implementation `uses`, so `ImplConst` is unresolvable from the
  interface. }

interface

uses sr_iface_const;

const
  KGood = IfaceConst;     { resolves via interface-uses }
  KBad  = ImplConst;      { must fail: visible only via implementation-uses }

implementation

uses sr_impl_const;

end.
