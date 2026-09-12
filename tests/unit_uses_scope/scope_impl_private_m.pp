unit Scope_Impl_Private_M;

interface

uses
  Scope_Impl_Private_P;

procedure SetPrivateVictim;

implementation

uses
  Scope_Impl_Private_X;

procedure SetPrivateVictim;
begin
  { Scope_Impl_Private_X declares PrivateVictim in its implementation only.
    The interface declaration from Scope_Impl_Private_P must win here. }
  PrivateVictim := 42
end;

end.
