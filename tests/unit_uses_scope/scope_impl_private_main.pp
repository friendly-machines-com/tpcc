program Scope_Impl_Private_Main;

uses
  Scope_Impl_Private_M, Scope_Impl_Private_P;

begin
  SetPrivateVictim;
  if PrivateVictim <> 42 then
    Halt(1)
end.
