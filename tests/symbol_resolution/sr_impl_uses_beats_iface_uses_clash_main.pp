program sr_impl_uses_beats_iface_uses_clash_main;

{ TARGET rule 2d: implementation-uses shadows interface-uses when both
  expose a symbol of the same name. The carrier sr_shadow_same_name has
  sr_iface_const in interface-uses and sr_impl_const in implementation-uses;
  both expose `Colliding`. Inside the implementation, the impl-uses value
  wins. }

uses sr_shadow_same_name;

begin
  if Read <> 20 then Halt(1)
end.
