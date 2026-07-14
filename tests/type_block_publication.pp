program TypeBlockPublication;

type
  TBase = class
  public
    function GetValue: LongInt; virtual;
  end;
  TChild = class(TBase)
  public
    function GetValue: LongInt; override;
  end;

function TBase.GetValue: LongInt;
begin
  GetValue := 10
end;

function TChild.GetValue: LongInt;
begin
  GetValue := inherited GetValue + 1
end;

begin
end.
