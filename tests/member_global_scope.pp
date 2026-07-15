program MemberGlobalScope;

procedure Select(Value: Integer); overload;
begin
end;

type
  TBox = class
    procedure Select(First, Second: TBox); overload;
    procedure Test;
  end;

procedure TBox.Select(First, Second: TBox);
begin
end;

procedure TBox.Test;
begin
  Select(1)
end;

begin
end.
