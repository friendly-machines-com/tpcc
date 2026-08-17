program NestedTypeVisibility;

type
  TOuter = class(TObject)
  public type
    TInner = class(TObject)
    private
      FInner: Integer;
    public
      constructor Create;
      function Get: Integer;
      procedure SetFrom(Value: TOuter);
    end;
  protected
    FValues: Integer;
  public
    constructor Create;
    function Sum: Integer;
  end;

constructor TOuter.TInner.Create;
begin
  FInner := 3
end;

function TOuter.TInner.Get: Integer;
begin
  Result := FInner
end;

procedure TOuter.TInner.SetFrom(Value: TOuter);
begin
  FInner := Value.Sum
end;

constructor TOuter.Create;
begin
  FValues := 7
end;

function TOuter.Sum: Integer;
var
  Inner: TInner;
begin
  Inner := TInner.Create;
  Result := FValues + Inner.Get;
  Inner.Free
end;

var
  Outer: TOuter;

begin
  Outer := TOuter.Create;
  if Outer.Sum <> 10 then
    Halt(1);
  Outer.Free
end.
