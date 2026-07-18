program IncompleteTypeCallableNormalization;

type
  TBase = class
    function GetCopy: TBase; virtual;
    function GetBase: TBase; virtual;
  end;

  TDerived = class(TBase)
    function GetCopy: TBase; override;
    { GetBase's stored result initially points at TBase's old LHS placeholder,
      while this later property spelling resolves TBase directly. Property
      compatibility must be decided only after the type block normalizes both. }
    property BaseCopy: TBase read GetBase;
  end;

function TBase.GetCopy: TBase;
begin
  Result := Self
end;

function TBase.GetBase: TBase;
begin
  Result := Self
end;

function TDerived.GetCopy: TBase;
begin
  Result := Self
end;

begin
end.
