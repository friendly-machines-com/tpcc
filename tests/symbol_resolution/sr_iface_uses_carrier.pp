unit sr_iface_uses_carrier;

{ Interface uses sr_iface_const (Shared=1), implementation uses sr_impl_const
  (Shared=2). Inside the implementation, TARGET rule 2d says the
  implementation-uses symbol shadows the interface-uses symbol. }

interface

uses sr_iface_const;

function ReadIface: Integer;
function ReadImpl: Integer;

implementation

uses sr_impl_const;

function ReadIface: Integer;
begin
  Result := IfaceConst
end;

function ReadImpl: Integer;
begin
  Result := ImplConst
end;

end.
