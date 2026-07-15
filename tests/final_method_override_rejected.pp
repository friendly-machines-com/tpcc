program FinalMethodOverrideRejected;

type
  TBase = class(TObject)
  public
{$ifdef TEST_INSTANCE}
    procedure Run; virtual;
{$endif}
{$ifdef TEST_CLASS}
    class procedure Run; virtual;
{$endif}
  end;

  TFinal = class(TBase)
  public
{$ifdef TEST_INSTANCE}
    procedure Run; override; final;
{$endif}
{$ifdef TEST_CLASS}
    class procedure Run; override; final;
{$endif}
  end;

  TTooFar = class(TFinal)
  public
{$ifdef TEST_INSTANCE}
    procedure Run; override;
{$endif}
{$ifdef TEST_CLASS}
    class procedure Run; override;
{$endif}
  end;

begin
end.
