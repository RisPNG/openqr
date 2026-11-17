defmodule OpenQR.Engine.MixProject do
  use Mix.Project

  def project do
    [
      app: :openqr_engine,
      version: "0.1.0",
      elixir: ">= 1.18.0",
      compilers: [:openqr] ++ Mix.compilers()
    ]
  end

  def application, do: []
end

defmodule Mix.Tasks.Compile.Openqr do
  use Mix.Task.Compiler

  def run(_args) do
    engine_root = System.get_env("OPENQR_SOURCE_DIR", Path.expand("../..", __DIR__))
    sdk_directory = System.get_env("OPENQR_SDK_DIR")
    include_directory = Path.join([:code.root_dir(), "usr", "include"])
    source_directory = if sdk_directory, do: __DIR__, else: engine_root

    build_directory =
      if sdk_directory,
        do: Path.join(Mix.Project.build_path(), "openqr-native"),
        else: Path.join(engine_root, "build")

    arguments = [
      "-S",
      source_directory,
      "-B",
      build_directory,
      "-DERL_NIF_INCLUDE_DIR=#{include_directory}"
    ]

    arguments =
      if sdk_directory,
        do: arguments ++ ["-DCMAKE_PREFIX_PATH=#{Path.expand(sdk_directory)}"],
        else: arguments ++ ["-DOPENQR_BUILD_ELIXIR=ON", "-DOPENQR_ELIXIR_BINDING_DIR=#{__DIR__}"]

    cache =
      case File.read(Path.join(build_directory, "CMakeCache.txt")) do
        {:ok, contents} ->
          contents
          |> String.split("\n")
          |> Enum.flat_map(fn line ->
            case Regex.run(~r/^([^#\/][^:]*):[^=]+=(.*)$/, line) do
              [_, name, value] -> [{name, String.trim_trailing(value, "\r")}]
              _ -> []
            end
          end)
          |> Map.new()

        {:error, :enoent} ->
          %{}
      end

    binding_matches? =
      if match?({:win32, _}, :os.type()),
        do: String.downcase(cache["OPENQR_ELIXIR_BINDING_DIR"] || "") == String.downcase(__DIR__),
        else: cache["OPENQR_ELIXIR_BINDING_DIR"] == __DIR__

    configure? =
      cache["ERL_NIF_INCLUDE_DIR"] != include_directory or
        if(sdk_directory,
          do: cache["CMAKE_PREFIX_PATH"] != Path.expand(sdk_directory),
          else: cache["OPENQR_BUILD_ELIXIR"] != "ON" or not binding_matches?
        )

    arguments =
      if Map.has_key?(cache, "CMAKE_BUILD_TYPE"),
        do: arguments,
        else: arguments ++ ["-DCMAKE_BUILD_TYPE=Release"]

    commands = if configure?, do: [["exec", "--", "cmake" | arguments]], else: []

    commands =
      commands ++
        [
          [
            "exec",
            "--",
            "cmake",
            "--build",
            build_directory,
            "--config",
            "Release",
            "--target",
            "openqr_elixir",
            "--parallel",
            "1"
          ]
        ]

    for command <- commands do
      case System.cmd("mise", command, cd: source_directory, into: IO.stream()) do
        {_, 0} -> :ok
        {_, status} -> Mix.raise("OpenQR native build failed with status #{status}")
      end
    end

    if match?({:win32, _}, :os.type()) do
      Mix.Project.ensure_structure()
      source_directory = Path.join(__DIR__, "priv")
      destination_directory = Path.join(Mix.Project.app_path(), "priv")
      identities = ~w(openqr-build-id.txt openqr-nif-build-id.txt)
      runtime_files = File.ls!(source_directory) -- identities

      refresh? =
        Enum.any?(identities, fn name ->
          File.read(Path.join(destination_directory, name)) !=
            {:ok, File.read!(Path.join(source_directory, name))}
        end) or
          Enum.any?(runtime_files, fn name ->
            not File.regular?(Path.join(destination_directory, name))
          end)

      if refresh? do
        File.mkdir_p!(destination_directory)

        for name <- runtime_files ++ identities do
          File.cp!(Path.join(source_directory, name), Path.join(destination_directory, name))
        end
      end
    else
      Mix.Project.build_structure()
    end

    {:ok, []}
  end
end
