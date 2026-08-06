program sr_iface_uses_shadows_via_carrier;

{ TARGET rule 2d exercised via a third-party unit: sr_iface_uses_carrier has
  sr_iface_const in its interface-uses and sr_impl_const in its
  implementation-uses. Inside the carrier's implementation both are visible,
  and the implementation-uses entry shadows the interface-uses entry. }

uses sr_iface_uses_carrier;

begin
  if ReadIface <> 10 then Halt(1);
  if ReadImpl <> 20 then Halt(2)
end.
