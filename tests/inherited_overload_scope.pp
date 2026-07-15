program InheritedOverloadScope;

type
  TBase = class
    procedure Select(Value: Integer); overload;
      external name 'p_base_select';
  end;

  TChild = class(TBase)
    procedure Select(Value: QWord); overload;
      external name 'p_child_select';
    procedure Test(I: Integer; Q: QWord);
  end;

procedure TChild.Test(I: Integer; Q: QWord);
begin
  Select(I);
  Select(Q)
end;

begin
end.
