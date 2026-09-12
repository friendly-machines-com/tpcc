unit Scope_Impl_Private_Absent;

interface

function ReadPrivate: Integer;

implementation

uses
  Scope_Impl_Private_X;

function ReadPrivate: Integer;
begin
  { PrivateVictim lives only in Scope_Impl_Private_X's implementation, so it
    must not be visible through `uses`. }
  Result := PrivateVictim
end;

end.
