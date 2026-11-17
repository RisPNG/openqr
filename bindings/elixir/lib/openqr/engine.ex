defmodule OpenQR.Engine do
  @on_load :load_nif

  def load_nif do
    :erlang.load_nif(
      Application.app_dir(:openqr_engine, "priv/openqr_nif") |> String.to_charlist(),
      0
    )
  end

  def generate(payload, options \\ []) when is_binary(payload) and is_list(options) do
    generate_native(payload, Map.new(options))
  end

  def generate_native(_payload, _options), do: :erlang.nif_error(:nif_not_loaded)
  def inspect_image(_encoded, _expected_payload), do: :erlang.nif_error(:nif_not_loaded)
  def styles(_category), do: :erlang.nif_error(:nif_not_loaded)
  def style_preview(_category, _style), do: :erlang.nif_error(:nif_not_loaded)
end
