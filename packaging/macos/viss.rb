class Viss < Formula
  desc "Expressive and lightning-fast native systems scripting language"
  homepage "https://github.com/Halva-developer/Viss"
  url "https://github.com/Halva-developer/Viss/archive/refs/tags/v0.2.2.tar.gz"
  version "0.2.2"
  license "MIT"

  depends_on "gcc" => :build

  def install
    system "g++", "-std=c++20", "-O3", "-pipe",
           "-I.", "-Ilibs", "-Isrc",
           "src/vissc.cpp", "-o", "bin/viss", "-pthread"

    bin.install "bin/viss"
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
