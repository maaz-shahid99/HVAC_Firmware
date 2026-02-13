import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:permission_handler/permission_handler.dart';
import '../ble_service.dart';
import 'onboarding_screen.dart';
import 'settings_page.dart';
import 'diagnostics_page.dart';
import 'qr_scanner_page.dart';
import '../widgets/connection_status_card.dart';
import '../widgets/disconnected_view.dart';
import '../widgets/quick_actions_card.dart';
import '../widgets/qr_scan_button.dart';
import '../widgets/commission_history_card.dart';
import '../widgets/logs_section.dart';
import '../widgets/bridge_auth_card.dart'; // Add this new import

class HomePage extends StatefulWidget {
  final VoidCallback onToggleTheme;

  const HomePage({super.key, required this.onToggleTheme});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> with TickerProviderStateMixin {
  bool _showOnboarding = true;

  @override
  void initState() {
    super.initState();
    _checkOnboarding();
    _requestPermissions();
  }

  Future<void> _checkOnboarding() async {
    final prefs = await SharedPreferences.getInstance();
    final hasSeenOnboarding = prefs.getBool('hasSeenOnboarding') ?? false;
    setState(() {
      _showOnboarding = !hasSeenOnboarding;
    });
  }

  Future<void> _completeOnboarding() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setBool('hasSeenOnboarding', true);
    setState(() {
      _showOnboarding = false;
    });
  }

  Future<void> _requestPermissions() async {
    await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.camera,
    ].request();
  }

  void _showQRScanner(BuildContext context) async {
    final result = await Navigator.push(
      context,
      MaterialPageRoute(builder: (context) => const QRScannerPage()),
    );

    if (result == null || !mounted) return;

    if (result is Map<String, String>) {
      final eui64 = result['eui64']!;
      final pskd = result['pskd']!;
      final bleService = Provider.of<BLEService>(context, listen: false);

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Commissioning $eui64...'),
          duration: const Duration(seconds: 1),
        ),
      );

      await bleService.commissionDevice(eui64, pskd);

      if (!mounted) return;

      final last = bleService.commissionHistory.isNotEmpty
          ? bleService.commissionHistory.last
          : null;

      final success = (last != null && last.eui64 == eui64 && last.success);

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(success
              ? '✅ Added successfully: $eui64'
              : '❌ Failed to add: $eui64'),
          backgroundColor: success ? Colors.green : Colors.red,
          duration: const Duration(seconds: 4),
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    if (_showOnboarding) {
      return OnboardingScreen(onComplete: _completeOnboarding);
    }

    return Scaffold(
      appBar: AppBar(
        title: const Text('Thread Commissioner'),
        actions: [
          Consumer<BLEService>(
            builder: (context, bleService, _) => IconButton(
              icon: Icon(
                bleService.isConnected
                    ? Icons.bluetooth_connected
                    : Icons.bluetooth,
                color: bleService.isConnected ? Colors.green : null,
              ),
              onPressed: () {
                Navigator.push(
                  context,
                  MaterialPageRoute(
                      builder: (context) => const DiagnosticsPage()),
                );
              },
            ),
          ),
          IconButton(
            icon: const Icon(Icons.settings),
            onPressed: () {
              Navigator.push(
                context,
                MaterialPageRoute(
                    builder: (context) =>
                        SettingsPage(onToggleTheme: widget.onToggleTheme)),
              );
            },
          ),
        ],
      ),
      body: Consumer<BLEService>(
        builder: (context, bleService, _) {
          return Column(
            children: [
              ConnectionStatusCard(bleService: bleService),
              Expanded(
                child: bleService.isConnected
                    ? SingleChildScrollView(
                  padding: const EdgeInsets.all(16),
                  // --- New: Gatekeeper UI Logic ---
                  child: bleService.authState == BridgeAuthState.authenticated
                      ? Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      QuickActionsCard(onScanQR: () => _showQRScanner(context)),
                      const SizedBox(height: 16),
                      QRScanButton(onTap: () => _showQRScanner(context)),
                      const SizedBox(height: 16),
                      CommissionHistoryCard(bleService: bleService),
                    ],
                  )
                      : BridgeAuthCard(bleService: bleService), // Displays Setup/Login UI
                )
                    : DisconnectedView(bleService: bleService),
              ),
              LogsSection(bleService: bleService),
            ],
          );
        },
      ),
    );
  }
}