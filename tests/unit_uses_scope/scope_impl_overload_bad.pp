program Scope_Impl_Overload_Bad;

uses
  Scope_Impl_Overload_O;

var
  S: ShortString;

begin
  S := 'x';
  { The implementation-only overload is private to Scope_Impl_Overload_O. }
  OverloadFoo(S)
end.
