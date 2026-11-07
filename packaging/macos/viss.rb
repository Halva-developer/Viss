class Viss < Formula
  desc "Expressive and lightning-fast native systems scripting language"
  homepage "https://github.com/Halva-developer/Viss"
  url "https://github.com/Halva-developer/Viss/archive/refs/tags/v0.4.2.tar.gz"
  version "0.4.2"
  license "MIT"

  depends_on "gcc" => :build

  def install
    mkdir_p "bin"
    system "g++", "-std=c++20", "-O3", "-pipe",
           "-I.", "-Ilibs", "-Isrc",
           "src/vissc.cpp", "-o", "bin/viss", "-pthread"
    system "g++", "-std=c++20", "-O3", "-pipe",
           "-I.", "-Ilibs", "-Isrc",
           "src/vsc_cli.cpp", "-o", "bin/vsc", "-pthread"

    bin.install "bin/viss"
    bin.install "bin/vsc"
    bin.install_symlink "vsc" => "ssi"
    (share/"viss/libs").install Dir["libs/*"]
    (include/"viss").install Dir["libs/*"]
  end

  test do
    (testpath/"hello.viss").write <<~EOS
      $import lib "io" as io
      !main {
          io.println("Hello from Viss on macOS!");
      }
    EOS
    system "#{bin}/viss", "run", testpath/"hello.viss"
  end
end
