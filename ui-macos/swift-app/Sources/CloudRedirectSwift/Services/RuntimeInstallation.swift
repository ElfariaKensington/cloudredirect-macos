import Foundation
import CloudRedirectSupport

enum RuntimeInstallationResult: Equatable {
    case installed
    case confirmationRequired(String)
}

@MainActor enum RuntimeInstallation {
    static func perform(confirmed: Bool = false,
                        diagnose: () async throws -> NativeCommandResult,
                        install: () async throws -> Void) async throws -> RuntimeInstallationResult {
        // Compatibility is advisory at install time. Consent applies only to
        // this attempt; never disable or persist a bypass of runtime checks.
        if !confirmed {
            do {
                let result = try await diagnose().json()
                guard result["ok"] as? Bool == true else {
                    throw AppError.message("The compatibility diagnostic did not confirm support for this Steam build.")
                }
            }
            catch is CancellationError { throw CancellationError() }
            catch { return .confirmationRequired(error.localizedDescription) }
        }
        try Task.checkCancellation()
        try await install()
        return .installed
    }
}
