import XCTest
import CloudRedirectSupport
@testable import CloudRedirectSwift

final class RuntimeInstallationTests: XCTestCase {
    private func diagnostic(_ json: String, status: Int32 = 0) -> NativeCommandResult {
        NativeCommandResult(data: Data(json.utf8), errors: "", status: status)
    }

    @MainActor func testValidatedBuildInstallsWithoutConfirmation() async throws {
        var installed = false
        let result = try await RuntimeInstallation.perform(
            diagnose: { self.diagnostic(#"{"ok":true}"#) },
            install: { installed = true })
        XCTAssertEqual(result, .installed)
        XCTAssertTrue(installed)
    }

    @MainActor func testUnknownBuildRequiresConsentBeforeAnyInstallation() async throws {
        var installed = false
        let message = "This Steam build has not been validated; hooks remain inactive"
        let result = try await RuntimeInstallation.perform(
            diagnose: { self.diagnostic("{\"ok\":false,\"message\":\"\(message)\"}", status: 1) },
            install: { installed = true })
        XCTAssertEqual(result, .confirmationRequired(message))
        XCTAssertFalse(installed)
    }

    @MainActor func testConfirmedInstallDoesNotRepeatFailingDiagnostic() async throws {
        var installed = false
        let result = try await RuntimeInstallation.perform(confirmed: true,
            diagnose: { XCTFail("An explicit confirmation must allow installation"); throw AppError.message("Unsupported build") },
            install: { installed = true })
        XCTAssertEqual(result, .installed)
        XCTAssertTrue(installed)
    }

    @MainActor func testConsentDoesNotCarryOverToNextAttempt() async throws {
        _ = try await RuntimeInstallation.perform(confirmed: true,
            diagnose: { self.diagnostic("{}") }, install: {})
        let result = try await RuntimeInstallation.perform(
            diagnose: { self.diagnostic(#"{"ok":false,"message":"New Steam build"}"#, status: 1) },
            install: { XCTFail("Each unverified attempt needs fresh consent") })
        XCTAssertEqual(result, .confirmationRequired("New Steam build"))
    }

    @MainActor func testUnavailableOrMalformedDiagnosticRequiresConsent() async throws {
        let unavailable = try await RuntimeInstallation.perform(
            diagnose: { throw AppError.message("Helper could not start") },
            install: { XCTFail("Missing diagnostics must not silently install") })
        XCTAssertEqual(unavailable, .confirmationRequired("Helper could not start"))
        let malformed = try await RuntimeInstallation.perform(
            diagnose: { self.diagnostic("not JSON", status: 1) },
            install: { XCTFail("Invalid diagnostics must not silently install") })
        guard case .confirmationRequired = malformed else { return XCTFail("Expected confirmation") }
    }

    @MainActor func testInstallFailuresAreNotConvertedToCompatibilityWarnings() async {
        for confirmed in [false, true] {
            do {
                _ = try await RuntimeInstallation.perform(confirmed: confirmed,
                    diagnose: { self.diagnostic(#"{"ok":true}"#) },
                    install: { throw AppError.message("Signing failed") })
                XCTFail("Installation errors must still fail")
            } catch { XCTAssertEqual(error.localizedDescription, "Signing failed") }
        }
    }

    @MainActor func testIncompleteOrInconsistentDiagnosticsNeverSilentlyInstall() async throws {
        let diagnostics = [
            diagnostic("{}"),
            diagnostic(#"{"ok":false,"message":"Self-test failed"}"#),
            diagnostic(#"{"ok":true}"#, status: 1)
        ]
        for diagnostic in diagnostics {
            let result = try await RuntimeInstallation.perform(
                diagnose: { diagnostic },
                install: { XCTFail("Only an explicit successful diagnostic may install without consent") })
            guard case .confirmationRequired = result else { return XCTFail("Expected confirmation") }
        }
    }

    @MainActor func testCancelledDiagnosticDoesNotPromptOrInstall() async {
        do {
            _ = try await RuntimeInstallation.perform(
                diagnose: { throw CancellationError() },
                install: { XCTFail("Cancellation must not install") })
            XCTFail("Expected cancellation")
        } catch { XCTAssertTrue(error is CancellationError) }
    }
}
