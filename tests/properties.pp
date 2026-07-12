program properties;

type
  TBox = object
  private
    FDirect: Integer;
    FValue: Integer;
    FItems: array[1..3] of Integer;
    function GetValue: Integer;
    procedure SetValue(Value: Integer);
    function GetItem(I: Integer): Integer;
    procedure SetItem(I: Integer; Value: Integer);
  public
    property Direct: Integer read FDirect write FDirect;
    property Value: Integer read GetValue write SetValue;
    property Items[I: Integer]: Integer read GetItem write SetItem; default;
  end;

type
  TChildBox = object(TBox)
  end;

  TGrid = object
  private
    FCells: array[0..3] of Integer;
    function GetCell(X, Y: Integer): Integer;
    procedure SetCell(X, Y: Integer; Value: Integer);
  public
    property Cells[X, Y: Integer]: Integer read GetCell write SetCell; default;
  end;

var
  Box: TBox;
  Child: TChildBox;
  Grid: TGrid;
  A: array[1..2] of Integer;
  S: ShortString;
  LongS: AnsiString;
  C: Char;
  AC: Char;
  CP: ^Char;
  P: ^Integer;
  X: Integer;
  SeenByte: Integer;
  SeenChar: Integer;

procedure MutateInteger(var Value: Integer);
begin
  Value := Value + 1
end;

procedure MutateChar(var Value: Char);
begin
  Value := Value
end;

procedure Mark(Value: Byte); overload;
begin
  SeenByte := 1
end;

procedure Mark(Value: Char); overload;
begin
  SeenChar := 1
end;

function TBox.GetValue: Integer;
begin
  GetValue := FValue
end;

procedure TBox.SetValue(Value: Integer);
begin
  FValue := Value
end;

function TBox.GetItem(I: Integer): Integer;
begin
  GetItem := FItems[I]
end;

procedure TBox.SetItem(I: Integer; Value: Integer);
begin
  FItems[I] := Value
end;

function TGrid.GetCell(X, Y: Integer): Integer;
begin
  GetCell := FCells[X * 2 + Y]
end;

procedure TGrid.SetCell(X, Y: Integer; Value: Integer);
begin
  FCells[X * 2 + Y] := Value
end;

begin
  Box.Direct := 10;
  Box.Value := 20;
  Box.Items[1] := 30;
  Box[2] := 40;
  Child[3] := 41;
  Grid.Cells[0, 1] := 42;
  Grid[1, 0] := 43;
  MutateInteger(Box.Direct);
  P := @Box.Direct;

  A[1] := 50;
  MutateInteger(A[1]);

  S := 'ab';
  C := S[1];
  S[2] := S[1];
  MutateChar(S[1]);
  UniqueString(LongS);
  LongS[1] := S[1];
  MutateChar(LongS[1]);
  CP := @LongS[1];
  AC := LongS[1];
  Mark(Byte(1));
  Mark(S[1]);

  X := Box.Direct + Box.Value + Box.Items[1] + Box[2] + Child[3] +
       Grid[0, 1] + Grid.Cells[1, 0] + A[1]
end.
