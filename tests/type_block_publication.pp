program TypeBlockPublication;

type
  TBase = class
  public
    Value: LongInt;
    procedure SetValue(NewValue: LongInt);
    function GetValue: LongInt; virtual;
  end;
  TChild = class(TBase)
  public
    procedure UseInheritedMembers;
    function GetValue: LongInt; override;
  end;

procedure TBase.SetValue(NewValue: LongInt);
begin
  Value := NewValue
end;

function TBase.GetValue: LongInt;
begin
  GetValue := 10
end;

procedure TChild.UseInheritedMembers;
begin
  SetValue(20);
  Value := Value + 1
end;

function TChild.GetValue: LongInt;
begin
  GetValue := inherited GetValue + 1
end;

begin
end.
