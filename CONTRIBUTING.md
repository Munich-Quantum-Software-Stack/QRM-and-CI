# Contributing to the Quantum Resource Manager

Thank you for your interest in contributing to Quantum Resource Manager! We appreciate your support and welcome your contributions.

Before you get started, please take a moment to read this document to understand the process for contributing to our project.

## How to Contribute

1. **Fork the Repository**: Click the "Fork" button on the top right of the repository's page to create your own copy.

2. **Clone Your Fork**: Clone your forked repository to your local machine:
   ```shell
   git clone https://gitlab-int.srv.lrz.de/lrz-qct-qis/quantum_intermediate_representation/your-fork.git
   ```

3. **Create a new branch**: Create a new branch for your contribution:
   ```shell
   git checkout -b custom-pass/name-of-pass
   ```

4. **Test**: Ensure that your changes work as intended and don't introduce any new issues.

5. **Format**: Apply the right formatting to your code base by manually triggering `clang-format` `pre-commit` hook.
   ```shell
   make format
   ```

6. **Commit Your Changes**: Commit your changes with a clear and concise message:
   ```shell
   git commit -m "Added name-of-pass"
   ```

7. **Push to Your Fork**: Push your changes to your fork on GitHub:
   ```shell
   git push origin custom-pass/name-of-pass
   ```

8. **Create a Pull Request**: Open a pull request from your branch to the `develop` branch in the original repository.

9. **Discuss and Revise**: Engage in any discussions or changes requested by the maintainers.

10. **Get Your Pull Request Merged**: Once your contribution is approved, it will be merged into the project.

## Code Style and Guidelines

Before making contributions, please familiarize yourself with our coding standards and guidelines. Kindly ensure that your code contributions include comments that adhere to the formatting and conventions expected by [Doxygen](https://www.doxygen.nl/manual/docblocks.html), as clear and consistent documentation is essential for maintaining and understanding the project.

## Reporting Issues

If you encounter any bugs or issues with the project, please report them on the [Issues](https://gitlab-int.srv.lrz.de/lrz-qct-qis/munich-quantum-compiler/quantum-resource-manager/-/issues) page.

## Code of Conduct

Please review our [Code of Conduct](CODE_OF_CONDUCT.md) to understand our community's expectations and behavior standards.

## Questions and Assistance

If you have any questions or need assistance with your contributions, please feel free to reach out to us via [email](mailto:jorge.echavarria@lrz.de) or create an issue in the repository.

Thank you for your contributions to the Quantum Resource Manager!

## License

By contributing to this project, you agree to the terms and conditions of the [License](LICENSE) for Quantum Resource Manager.
