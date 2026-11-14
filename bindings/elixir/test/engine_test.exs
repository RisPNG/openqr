defmodule OpenQR.EngineTest do
  use ExUnit.Case, async: true

  test "generates literal payload bytes through the native engine" do
    payload = "  OpenQR\0\n"

    assert {:ok, %{results: [entry], seed: 42}} =
             OpenQR.Engine.generate(payload, version: 3, seed: 42)

    assert entry.payload == payload
    assert entry.verified_frames == entry.total_frames
    assert entry.image =~ <<137, 80, 78, 71>>
    assert entry.error_correction in ["H", "Q", "M", "L"]
    assert entry.svg =~ "<svg"
  end

  test "passes artwork and returns correction-budget metadata" do
    reference =
      Base.decode64!(
        "iVBORw0KGgoAAAANSUhEUgAAAAYAAAAFCAYAAABmWJ3mAAAAHklEQVR4nGMUMYr6zwAFb84tY4SxmWAMdIBTgnQAAM/bBAZ/BQtiAAAAAElFTkSuQmCC"
      )

    assert {:ok, %{results: [entry], reference_input: reusable}} =
             OpenQR.Engine.generate("https://example.com",
               reference: reference,
               version: 8,
               seed: 42,
               color: false
             )

    assert byte_size(reusable) > 0
    assert length(entry.block_errors) == length(entry.block_capacities)

    assert Enum.all?(Enum.zip(entry.block_errors, entry.block_capacities), fn {errors, capacity} ->
             errors <= capacity - 2
           end)
  end

  test "returns native validation failures as errors" do
    assert {:error, reason} = OpenQR.Engine.generate("QR", scale: 4)
    assert reason =~ "Scale"
  end

  test "exposes native styles and previews" do
    assert map_size(OpenQR.Engine.styles(:body)) == 22
    assert {:ok, svg} = OpenQR.Engine.style_preview(:eye_frame, "rounded")
    assert svg =~ "<svg"
    assert {:error, _} = OpenQR.Engine.style_preview(:body, "missing")
  end
end
