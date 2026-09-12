unit Scope_Impl_Private_Self;

interface

function ReadPrivateFromSelf: Integer;

implementation

const
  SelfPrivate = 77;

function ReadPrivateFromSelf: Integer;
begin
  { SelfPrivate is implementation-only, but this is the unit itself, so it
    must be visible here. }
  Result := SelfPrivate
end;

end.
