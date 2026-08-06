unit sr_shadow_same_name;

{ Both interface-uses and implementation-uses expose a symbol named
  `Colliding`. TARGET rule 2d: inside the implementation, the
  implementation-uses entry wins. }

interface

uses sr_iface_const;

function Read: Integer;

implementation

uses sr_impl_const;

function Read: Integer;
begin
  Result := Colliding
end;

end.
