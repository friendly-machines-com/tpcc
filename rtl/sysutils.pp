unit sysutils;
interface

type
  Exception = class(TObject)
  private
    FMessage: String;
    FHelpContext: LongInt;
  public
    constructor Create(const Msg: String);
    constructor CreateHelp(const Msg: String; AHelpContext: LongInt);
    property HelpContext: LongInt read FHelpContext write FHelpContext;
    property Message: String read FMessage write FMessage;
  end;

function Supports(a: TObject; b: TClass): Boolean; external nil name '::u_system::p_supports';

implementation

constructor Exception.Create(const Msg: String);
begin
  FMessage := Msg;
  FHelpContext := 0
end;

constructor Exception.CreateHelp(const Msg: String; AHelpContext: LongInt);
begin
  FMessage := Msg;
  FHelpContext := AHelpContext
end;

end.
