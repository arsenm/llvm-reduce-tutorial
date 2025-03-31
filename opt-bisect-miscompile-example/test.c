// Demonstrate miscompile with 'buggy<miscompile-icmp-slt-to-sle>'
int main(int argc, const char* argv[]) {
  return argc < 2;
}
