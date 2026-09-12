program Scope_Impl_Private_Self_Main;

uses
  Scope_Impl_Private_Self;

begin
  if ReadPrivateFromSelf <> 77 then
    Halt(1)
end.
