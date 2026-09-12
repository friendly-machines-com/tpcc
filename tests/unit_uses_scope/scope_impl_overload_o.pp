unit Scope_Impl_Overload_O;

interface

procedure OverloadFoo(a: Integer); overload;
procedure CallOwnOverload;

implementation

procedure OverloadFoo(a: ShortString); overload;
begin
end;

procedure OverloadFoo(a: Integer);
begin
end;

procedure CallOwnOverload;
var
  S: ShortString;
begin
  S := 'x';
  { The implementation-only overload is part of this unit's own overload
    family, so it is callable here. }
  OverloadFoo(S)
end;

end.
